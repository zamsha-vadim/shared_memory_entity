#ifndef SME_FUTEX_H
#define SME_FUTEX_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include "sme/sme_export.h"

namespace sme {

using FutexValueType = uint32_t;

enum class SME_EXPORT FutexResult { kCompleted, kTimeout };
enum class SME_EXPORT PiFutexResult { kCompleted, kTimeout, kOwnerDied };

auto SME_EXPORT WaitFutex(std::atomic<FutexValueType>& addr,
                          FutexValueType check_value,
                          const std::chrono::milliseconds& timeout =
                              std::chrono::milliseconds::max()) -> FutexResult;

auto SME_EXPORT WakeFutex(std::atomic<FutexValueType>& addr, uint32_t waiter_number = 1)
    -> uint32_t;

auto SME_EXPORT LockPiFutex(std::atomic<FutexValueType>& addr,
                            const std::chrono::milliseconds& timeout =
                                std::chrono::milliseconds::max()) -> PiFutexResult;
void SME_EXPORT UnlockPiFutex(std::atomic<FutexValueType>& addr);
void SME_EXPORT SetConsistentPiFutex(std::atomic<FutexValueType>& addr) noexcept;

auto SME_EXPORT IsPiFutexOwnerDied(FutexValueType value) noexcept -> bool;
auto SME_EXPORT HasPiFutexWaiters(FutexValueType value) noexcept -> bool;

}  // namespace sme

#endif // SME_INTERNAL_FUTEX_H
