#ifndef SME_MSP_MAP_H
#define SME_MSP_MAP_H

#include <map>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename Key, class T, class Compare = std::less<Key>>
using map = std::map<Key, T, Compare, MemorySpaceAllocator<std::pair<Key, T>>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_MAP_H
