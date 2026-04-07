#ifndef SME_MDM_VECTOR_H
#define SME_MDM_VECTOR_H

#include <vector>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename T>
using vector = std::vector<T, MemoryDomainAllocator<T>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_VECTOR_H
