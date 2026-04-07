#ifndef SME_MDM_QUEUE_H
#define SME_MDM_QUEUE_H

#include <queue>

#include "sme/mdm/deque.h"

namespace sme {
namespace mdm {

template <typename T>
using queue = std::queue<T, sme::mdm::deque<T>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_QUEUE_H
