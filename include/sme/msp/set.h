#ifndef SME_MSP_SET_H
#define SME_MSP_SET_H

#include <set>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename Key, class Compare = std::less<Key>>
using set = std::set<Key, Compare, MemorySpaceAllocator<Key>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_SET_H
