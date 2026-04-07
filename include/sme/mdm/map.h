#ifndef SME_MDM_MAP_H
#define SME_MDM_MAP_H

#include <map>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename Key, class T, class Compare = std::less<Key>>
using map = std::map<Key, T, Compare, MemoryDomainAllocator<std::pair<const Key, T>>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_MAP_H
