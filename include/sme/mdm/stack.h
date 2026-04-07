#ifndef SME_MDM_STACK_H
#define SME_MDM_STACK_H

#include <stack>

#include "sme/mdm/deque.h"

namespace sme {
namespace mdm {

template <typename T>
using stack = std::stack<T, sme::mdm::deque<T>>;

}  // namespace mdm
}  // namespace sme

#endif  //  SME_MDM_STACK_H
