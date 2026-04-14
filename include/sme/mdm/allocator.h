#ifndef SME_MDM_ALLOCATOR_H
#define SME_MDM_ALLOCATOR_H

#include <cstddef>

#include "sme/basic_allocator.h"
#include "sme/mem_domain.h"
#include "sme/mem_ptr.h"

namespace sme {
namespace mdm {

template <typename T>
using MemoryDomainAllocator = BasicAllocator<T, MemoryDomain>;

using MemoryDomainByteAllocator = BasicAllocator<std::byte, MemoryDomain>;

template <typename ContainerType>
using ItemAllocator = MemoryDomainAllocator<typename ContainerType::value_type>;

template <typename ContainerType>
auto SME_EXPORT CreateItemAllocatorFor(MemoryDomain& mem_domain) noexcept
    -> ItemAllocator<ContainerType>
{
    return ItemAllocator<ContainerType>{mem_domain};
}

}  // namespace mdm
}  // namespace sme

#endif  // SME_MDM_ALLOCATOR_H
