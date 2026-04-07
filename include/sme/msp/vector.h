#ifndef SME_MSP_VECTOR_H
#define SME_MSP_VECTOR_H

#include <vector>

#include "sme/msp/allocator.h"

namespace sme {
namespace msp {

template <typename T>
using vector = std::vector<T, MemorySpaceAllocator<T>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_VECTOR_H
