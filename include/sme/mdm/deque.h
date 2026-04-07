#ifndef SME_MDM_DEQUE_H
#define SME_MDM_DEQUE_H

#include <deque>

#include "sme/mdm/allocator.h"

namespace sme {
namespace mdm {

template <typename T>
using deque = std::deque<T, MemoryDomainAllocator<T>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_DEQUE_H
