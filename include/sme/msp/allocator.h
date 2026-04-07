#ifndef SME_MSP_ALLOCATOR_H
#define SME_MSP_ALLOCATOR_H

#include "sme/basic_allocator.h"
#include "sme/mem_space.h"

namespace sme {
namespace msp {

template <typename T>
using MemorySpaceAllocator = BasicAllocator<T, MemorySpace>;

template <typename T>
auto CreateValueAllocatorFor(Pointer<MemorySpace> mem_space) noexcept -> auto
{
    return MemorySpaceAllocator<typename T::value_type>{std::move(mem_space)};
}

}  // namespace msp
}  // namespace sme

#endif  // SME_MSP_ALLOCATOR_H
