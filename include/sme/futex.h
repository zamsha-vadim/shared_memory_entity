#ifndef SME_FUTEX_H
#define SME_FUTEX_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include "sme/sme_export.h"

namespace sme {

enum class SME_EXPORT FutexResult { kCompleted, kTimeout };

auto SME_EXPORT FutexWait(std::atomic<uint32_t>& addr,
                          uint32_t check_value,
                          const std::chrono::milliseconds& ms_timeout =
                              std::chrono::milliseconds::max()) -> FutexResult;

auto SME_EXPORT FutexWake(std::atomic<uint32_t>& addr, uint32_t waiter_number = 1)
    -> uint32_t;

}  // namespace sme

#endif // SME_INTERNAL_FUTEX_H
