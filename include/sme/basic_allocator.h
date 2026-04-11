#ifndef SME_BASIC_ALLOCATOR_H
#define SME_BASIC_ALLOCATOR_H

#include <type_traits>

#include "sme/mem_ptr.h"
#include "sme/sme_export.h"

namespace sme {

// class AllocationContext

template <typename MemoryAreaType>
class SME_EXPORT AllocationContext final {
   public:
    using AreaPointer = Pointer<MemoryAreaType>;

   public:
    explicit AllocationContext(MemoryAreaType& area);

    AllocationContext(const AllocationContext&) = delete;
    AllocationContext(AllocationContext&&) = delete;
    auto operator=(const AllocationContext&) -> AllocationContext& = delete;
    auto operator=(AllocationContext&&) -> AllocationContext& = delete;

    ~AllocationContext();

    void Acquire();
    void Acquire(MemoryAreaType& area);
    void Release();
    auto IsAcquired() noexcept -> bool;
    static auto CanAcquire() noexcept -> bool;

    auto GetArea() const noexcept -> AreaPointer;
    static auto GetCurrentArea() noexcept -> AreaPointer;

    static auto IsPresented() noexcept -> bool;

   private:
    bool acquired_{false};

    AreaPointer area_;
    static thread_local AreaPointer
        glob_area_;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
};

// class BasicAllocator

template <typename T, typename MemoryAreaType>
class SME_EXPORT BasicAllocator {
   public:
    using value_type = T;
    using reference = value_type&;
    using const_reference = const value_type&;

    using pointer = Pointer<value_type>;
    using const_pointer = Pointer<const value_type>;
    using void_pointer = Pointer<void>;
    using const_void_pointer = Pointer<const void>;

    using difference_type = typename pointer::difference_type;
    using size_type = size_t;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    using is_always_equal = std::false_type;

    template <typename U>
    struct rebind {
        using other = BasicAllocator<U, MemoryAreaType>;
    };

   public:
    BasicAllocator();
    explicit BasicAllocator(MemoryAreaType& area);

    BasicAllocator(const BasicAllocator&) noexcept = default;
    BasicAllocator(BasicAllocator&&) noexcept = default;

    template <typename U>
    explicit BasicAllocator(const BasicAllocator<U, MemoryAreaType>& other) noexcept;

    template <typename U>
    explicit BasicAllocator(BasicAllocator<U, MemoryAreaType>&& other) noexcept;

    auto operator=(const BasicAllocator&) noexcept -> BasicAllocator& = default;
    auto operator=(BasicAllocator&&) noexcept -> BasicAllocator& = default;

    template <typename U>
    auto operator=(const BasicAllocator<U, MemoryAreaType>& other) noexcept
        -> BasicAllocator&;

    template <typename U>
    auto operator=(BasicAllocator<U, MemoryAreaType>&& other) noexcept -> BasicAllocator&;

    ~BasicAllocator() = default;

    auto IsValid() const noexcept -> bool { return (area_ != nullptr); }

    auto GetMemoryArea() const noexcept -> Pointer<MemoryAreaType> { return area_; }

    [[nodiscard]] auto allocate(size_type size) -> pointer;

    // WARNING: not implemented
    //[[nodiscard]] auto allocate_at_least(size_type n)
    //    -> std::allocation_result<pointer, std::size_t> {}

    void deallocate(pointer ptr, size_type size) noexcept;

    void construct(pointer ptr, const_reference val);

   private:
    Pointer<MemoryAreaType> area_;
};

// BasicAllocator utility functions

template <typename T, typename MemoryAreaType>
inline auto SME_EXPORT operator==(const BasicAllocator<T, MemoryAreaType>& lhs,
                                  const BasicAllocator<T, MemoryAreaType>& rhs) noexcept
    -> bool
{
    return lhs.GetMemoryArea() == rhs.GetMemoryArea();
}

template <typename T, typename MemoryAreaType>
inline auto SME_EXPORT operator!=(const BasicAllocator<T, MemoryAreaType>& lhs,
                                  const BasicAllocator<T, MemoryAreaType>& rhs) noexcept
    -> bool
{
    return !(lhs == rhs);
}

}  // namespace sme

#include "sme/basic_allocator.tcc"

#endif  // SME_BASIC_ALLOCATOR_H
