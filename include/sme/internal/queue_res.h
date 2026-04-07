#ifndef SME_INTERNAL_QUEUE_RES_H
#define SME_INTERNAL_QUEUE_RES_H

#include "sme/sme_export.h"

namespace sme {

enum class QueueResult { kFailed, kSuccessful, kTimeout, kRejected };

} // sme

#endif //SME_INTERNAL_QUEUE_RES_H
