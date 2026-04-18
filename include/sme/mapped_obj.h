#ifndef SME_MAPPED_OBJ_H
#define SME_MAPPED_OBJ_H

#include <new>
#include <stdexcept>
#include <utility>

#include "sme/mem_domain.h"
#include "sme/mem_map.h"
#include "sme/mem_space.h"
#include "sme/sme_export.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-const-cast,
// cppcoreguidelines-owning-memory)

namespace sme {

template <typename T>
auto SME_EXPORT EnsureAddress(MemoryMap& mem_map, size_t ofs) -> void*
{
    if ((ofs + sizeof(T)) > mem_map.GetSize())
        throw std::invalid_argument(
            "The object size is out of bounds the memory location");

    char* addr = static_cast<char*>(mem_map.GetAddress()) + ofs;
    if ((reinterpret_cast<uintptr_t>(addr) % alignof(T)) != 0)
        throw std::invalid_argument(
            "The memory address is not aligned for object location");

    return addr;
}

template <typename T>
auto SME_EXPORT EnsureAddress(const MemoryMap& mem_map, size_t ofs) -> const void*
{
    return EnsureAddress<T>(const_cast<MemoryMap&>(mem_map), ofs);
}

template <typename T, typename... Arg>
[[nodiscard]] auto SME_EXPORT Construct(MemoryMap& mem_map, size_t ofs, Arg&&... args) -> T*
{
    if ((ofs % alignof(T)) != 0)
        throw std::logic_error("Offset must conform type alignment");

    auto* addr = EnsureAddress<T>(mem_map, ofs);

    return new (addr) T{std::forward<Arg>(args)...};
}

[[nodiscard]] auto SME_EXPORT ConstructMemorySpace(
    MemoryMap& mem_map,
    size_t ofs = 0,
    Synchronizer::Type sync_type = Synchronizer::Type::kShared) -> MemorySpace*;

[[nodiscard]] auto SME_EXPORT GetMemorySpace(MemoryMap& mem_map, size_t ofs = 0)
    -> MemorySpace&;

template <typename T>
[[nodiscard]] auto SME_EXPORT GetObject(MemoryMap& mem_map, size_t ofs) -> T&
{
    auto* addr = EnsureAddress<T>(mem_map, ofs);

    return *std::launder(reinterpret_cast<T*>(addr));
}

template <typename T>
[[nodiscard]] auto SME_EXPORT GetObject(const MemoryMap& mem_map, size_t ofs) -> const T&
{
    const auto* addr = EnsureAddress<T>(mem_map, ofs);

    return *std::launder(reinterpret_cast<const T*>(addr));
}

class SME_EXPORT MemoryUnmapper {
   public:
    MemoryUnmapper() = default;
    MemoryUnmapper(size_t size) noexcept;

    auto GetSize() const noexcept -> size_t;
    void operator()(char* ptr) const noexcept;

   private:
    size_t size_{0};
};

using MappedMemoryUniquePtr = std::unique_ptr<char, MemoryUnmapper>;

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-const-cast,
// cppcoreguidelines-owning-memory)

#endif  // SME_MAPPED_OBJ_H
