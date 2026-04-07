#include "sme/mapped_obj.h"

#include <sys/mman.h>

#include <cassert>

namespace sme {

template <>
auto SME_EXPORT Construct<MemorySpace>(MemoryMap& mem_map, size_t ofs) -> MemorySpace*
{
    auto* addr = EnsureAddress<MemorySpace>(mem_map, ofs);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    char* space_addr = static_cast<char*>(addr) + sizeof(MemorySpace);
    auto space_size = mem_map.GetSize() - ofs - sizeof(MemorySpace);

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    return new (addr) MemorySpace{space_addr, space_size, Synchronizer::Type::kShared};
}

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
