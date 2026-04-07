#ifndef SME_MDM_FORWARD_LIST_H
#define SME_MDM_FORWARD_LIST_H

#include <forward_list>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename T>
using forward_list = std::forward_list<T, MemoryDomainAllocator<T>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_FORWARD_LIST_H
