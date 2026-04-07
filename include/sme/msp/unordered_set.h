#ifndef SME_MSP_UNORDERED_SET_H
#define SME_MSP_UNORDERED_SET_H

#include <unordered_set>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using unordered_set =
    std::unordered_set<Key, Hash, KeyEqual, MemorySpaceAllocator<const Key>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_UNORDERED_SET_H
