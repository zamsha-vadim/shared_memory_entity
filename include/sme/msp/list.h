#ifndef SME_MSP_LIST_H
#define SME_MSP_LIST_H

#include <list>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename T>
using list = std::list<T, MemorySpaceAllocator<T>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_LIST_H
