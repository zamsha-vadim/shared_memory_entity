#ifndef SME_MSP_FORWARD_LIST_H
#define SME_MSP_FORWARD_LIST_H

#include <forward_list>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename T>
using forward_list = std::forward_list<T, MemorySpaceAllocator<T>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_FORWARD_LIST_H
