#ifndef SME_MSP_QUEUE_H
#define SME_MSP_QUEUE_H

#include <queue>

#include "sme/msp/deque.h"

namespace sme {
namespace msp {

template <typename T>
using queue = std::queue<T, sme::msp::deque<T>>;

}  // namespace msp
}  // namespace sme

#endif  //  SME_MSP_QUEUE_H
