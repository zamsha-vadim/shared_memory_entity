#ifndef SME_MDM_UNIQUE_PTR_H
#define SME_MDM_UNIQUE_PTR_H

#include <memory>

#include "sme/alloc_util.h"
#include "sme/mem_domain.h"
#include "sme/sme_export.h"

namespace sme {
namespace mdm {

template <typename T>
using unique_ptr = std::unique_ptr<T, sme::Deleter<T, MemoryDomain>>;

}  // namespace mdm
}  // namespace sme

#endif // SME_MDM_UNIQUE_PTR_H
