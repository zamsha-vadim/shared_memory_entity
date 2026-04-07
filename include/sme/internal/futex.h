#ifndef SME_INTERNAL_FUTEXT_H
#define SME_INTERNAL_FUTEXT_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>

#include "sme/sme_export.h"

namespace sme {

enum class FutexResult { kCompleted, kTimeout };

auto SME_EXPORT FutexWait(std::atomic<uint32_t>* addr,
                          uint32_t check_value,
                          const std::chrono::milliseconds& ms_timeout =
                              std::chrono::milliseconds::max()) -> FutexResult;

auto SME_EXPORT FutexWake(std::atomic<uint32_t>* addr, uint32_t waiter_number)
    -> uint32_t;

}  // namespace sme

#endif // SME_INTERNAL_FUTEXT_H
