#ifndef SME_MDM_SET_H
#define SME_MDM_SET_H

#include <set>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename Key, class Compare = std::less<Key>>
using set = std::set<Key, Compare, MemoryDomainAllocator<Key>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_SET_H
