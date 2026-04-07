#ifndef SME_MDM_UNORDERED_SET_H
#define SME_MDM_UNORDERED_SET_H

#include <unordered_set>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
using unordered_set = std::unordered_set<Key, Hash, KeyEqual, MemoryDomainAllocator<Key>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_UNORDERED_SET_H
