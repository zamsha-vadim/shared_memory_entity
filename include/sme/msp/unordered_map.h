#ifndef SME_MSP_UNORDERED_MAP_H
#define SME_MSP_UNORDERED_MAP_H

#include <unordered_set>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename Key,
          typename T,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
using unordered_map = std::
    unordered_map<Key, T, Hash, KeyEqual, MemorySpaceAllocator<std::pair<const Key, T>>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_UNORDERED_MAP_H
