#ifndef SME_MS_STACK_H
#define SME_MS_STACK_H

#include <stack>

#include "sme/msp/deque.h"

namespace sme {
namespace ms {

template <typename T>
using stack = std::stack<T, sme::msp::deque<T>>;

}  // namespace ms
}  // namespace sme

#endif  //  SME_MS_STACK_H
