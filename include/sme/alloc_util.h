#ifndef SME_ALLOC_UTIL_H
#define SME_ALLOC_UTIL_H

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "sme/mem_ptr.h"
#include "sme/sme_export.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,)

namespace sme {

template <typename T, typename = void>
struct IsAllocator : public std::false_type {
};

template <typename T>
struct IsAllocator<T, std::void_t<decltype(std::declval<T>().Allocate(1))>>
    : public std::true_type {
};

template <typename T, typename = void>
struct IsDeallocator : public std::false_type {
};

template <typename T>
struct IsDeallocator<
    T,
    std::void_t<decltype(std::declval<T>().Deallocate(sme::Pointer<void>{}))>>
    : public std::true_type {
};

template <typename T, typename MemoryAreaType, typename... Arg>
[[nodiscard]] auto SME_EXPORT Create(MemoryAreaType& mem_area, Arg&&... args) -> T*
{
    static_assert(IsAllocator<MemoryAreaType>(),
                  "MemoryAreaType template argument must have method \"Pointer<void> "
                  "Allocate(size_t data_size)\"");

    auto mem = mem_area.Allocate(sizeof(T));
    if (mem == nullptr)
        return nullptr;

    auto* alloc_addr = mem.GetAddress();
    assert((reinterpret_cast<uintptr_t>(alloc_addr) % alignof(T)) == 0);

    try {
        return new (alloc_addr) T{std::forward<Arg>(args)...};
    } catch (...) {
        mem_area.Deallocate(mem);
        throw;
    }
}

template <typename T, typename MemoryAreaType>
void SME_EXPORT Delete(MemoryAreaType& mem_area, Pointer<T>& obj) noexcept
{
    static_assert(IsDeallocator<MemoryAreaType>(),
                  "MemoryAreaType template argument must have method \"void "
                  "Deallocate(Pointer<void>&)\"");

    if (obj == nullptr)
        return;

    if constexpr (std::is_destructible_v<T>)
        obj->~T();

    mem_area.Deallocate(obj);
    obj.Reset();
}

// NOLINTBEGIN(cppcoreguidelines-rvalue-reference-param-not-moved,-warnings-as-errors)

template <typename T, typename MemoryAreaType>
void SME_EXPORT Delete(MemoryAreaType& mem_area, Pointer<T>&& obj) noexcept 
{
    Delete(mem_area, obj);
}

// NOLINTEND(cppcoreguidelines-rvalue-reference-param-not-moved,-warnings-as-errors)

template <typename T, typename MemoryAreaType>
class SME_EXPORT Deleter final {
    static_assert(IsDeallocator<MemoryAreaType>(),
                  "MemoryAreaType template argument must have method \"void "
                  "Deallocate(Pointer<void>&&)\"");

   public:
    using pointer = Pointer<T>;

   public:
    explicit Deleter(MemoryAreaType& mem_area) noexcept : mem_area_{&mem_area} {}
    Deleter() = default;

    auto GetMemoryArea() const noexcept -> const MemoryAreaType&
    {
        assert(IsValid());
        return *mem_area_;
    }

    auto GetMemoryArea() noexcept -> MemoryAreaType&
    {
        assert(IsValid());
        return *mem_area_;
    }

    void operator()(pointer obj) noexcept {
        assert(IsValid());
        if (mem_area_ == nullptr)
            return;

        Delete<T, MemoryAreaType>(*mem_area_, obj);
    }

    void Reset() noexcept { mem_area_ = nullptr; }

    auto IsValid() const noexcept -> bool { return mem_area_ != nullptr; }

   private:
    Pointer<MemoryAreaType> mem_area_{};
};

template <typename T,
          typename MemoryAreaType,
          typename DeleterType = sme::Deleter<T, MemoryAreaType>,
          typename... Arg>
[[nodiscard]] auto SME_EXPORT MakeUnique(MemoryAreaType& mem_area, Arg&&... args)
    -> std::unique_ptr<T, DeleterType>
{
    static_assert(IsAllocator<MemoryAreaType>(),
                  "MemoryAreaType template argument must have method \"Pointer<void> "
                  "Allocate(size_t data_size)\"");

    auto* obj = Create<T>(mem_area, std::forward<Arg>(args)...);
    return std::unique_ptr<T, DeleterType>{obj, DeleterType{mem_area}};
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,)

#endif  // SME_ALLOC_UTIL_H
