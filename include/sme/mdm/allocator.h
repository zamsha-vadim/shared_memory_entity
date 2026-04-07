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

template <typename T>
using MemoryDomainByteAllocator = BasicAllocator<std::byte, MemoryDomain>;

template <typename T>
auto SME_EXPORT CreateValueAllocatorFor(MemoryDomain& mem_domain) noexcept -> auto
{
    return MemoryDomainAllocator<typename T::value_type>{mem_domain};
}

}  // namespace mdm
}  // namespace sme

#endif  // SME_MDM_ALLOCATOR_H
