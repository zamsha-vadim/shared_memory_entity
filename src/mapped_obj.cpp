#include "sme/mapped_obj.h"

#include <sys/mman.h>

#include <cassert>

namespace sme {

namespace {

void CheckMemorySpaceOffsetAligned(size_t ofs)
{
    if ((ofs % alignof(MemorySpace)) != 0)
        throw std::logic_error("Offset must conform sme::MemorySpace class alignment");
}

}  // namespace

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines)

auto SME_EXPORT ConstructMemorySpace(MemoryMap& mem_map,
                                     size_t ofs,
                                     Synchronizer::Type sync_type) -> MemorySpace*
{
    CheckMemorySpaceOffsetAligned(ofs);

    auto* addr = EnsureAddress<MemorySpace>(mem_map, ofs);

    char* space_addr = static_cast<char*>(addr) + sizeof(MemorySpace);
    auto space_size = mem_map.GetSize() - ofs - sizeof(MemorySpace);

    return new (addr) MemorySpace{space_addr, space_size, sync_type};
}

auto SME_EXPORT GetMemorySpace(MemoryMap& mem_map, size_t ofs) -> MemorySpace&
{
    auto& mem_space = GetObject<MemorySpace>(mem_map, ofs);

    if (!MemorySpace::IsValidObjectId(mem_space))
        throw std::runtime_error("Invalid memory struct for sme::MemorySpace class");

    return mem_space;    
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines)

// class MemoryUnmapper

MemoryUnmapper::MemoryUnmapper(size_t size) noexcept : size_{size}
{
    assert(size_ != 0);
}

auto MemoryUnmapper::GetSize() const noexcept -> size_t
{
    return size_;
}

void MemoryUnmapper::operator()(char* ptr) const noexcept
{
    assert(size_ != 0);

    [[maybe_unused]] auto res = munmap(ptr, size_);
    assert(res == 0);
}

}  // namespace sme
