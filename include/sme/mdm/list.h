#ifndef SME_MDM_LIST_H
#define SME_MDM_LIST_H

#include <list>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename T>
using list = std::list<T, MemoryDomainAllocator<T>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_LIST_H
