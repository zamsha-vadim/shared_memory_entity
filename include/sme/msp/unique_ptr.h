#ifndef SME_MSP_UNIQUE_PTR_H
#define SME_MSP_UNIQUE_PTR_H

#include <memory>

#include "sme/alloc_util.h"
#include "sme/mem_space.h"
#include "sme/sme_export.h"

namespace sme {
namespace msp {

template <typename T>
using unique_ptr = std::unique_ptr<T, sme::Deleter<T, MemorySpace>>;

}  // namespace msp
}  // namespace sme

#endif // SME_MSP_UNIQUE_PTR_H

