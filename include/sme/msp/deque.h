#ifndef SME_MSP_DEQUE_H
#define SME_MSP_DEQUE_H

#include <deque>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename T>
using deque = std::deque<T, MemorySpaceAllocator<T>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_DEQUE_H
