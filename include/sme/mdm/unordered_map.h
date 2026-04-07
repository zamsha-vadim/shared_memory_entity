#ifndef SME_MDM_UNORDERED_MAP_H
#define SME_MDM_UNORDERED_MAP_H

#include <unordered_set>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename Key,
          typename T,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
using unordered_map = std::
    unordered_map<Key, T, Hash, KeyEqual, MemoryDomainAllocator<std::pair<const Key, T>>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_UNORDERED_MAP_H
