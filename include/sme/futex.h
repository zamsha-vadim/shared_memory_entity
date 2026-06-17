#ifndef SME_FUTEX_H
#define SME_FUTEX_H

#include <atomic>
#include <chrono>
#include <cstdint>

#include "sme/sme_export.h"

namespace sme {

using FutexValueType = uint32_t;

constexpr std::chrono::milliseconds kInfiniteTimeout{std::chrono::milliseconds::max()};

enum class SME_EXPORT FutexResult { kCompleted, kTimeout };
enum class SME_EXPORT PiFutexResult { kCompleted, kTimeout, kOwnerDied };

auto SME_EXPORT WakeFutex(std::atomic<FutexValueType>& var, uint32_t waiter_number = 1)
    -> uint32_t;

auto SME_EXPORT WaitFutex(std::atomic<FutexValueType>& var,
                          FutexValueType check_value,
                          const std::chrono::milliseconds& timeout = kInfiniteTimeout)
    -> FutexResult;

auto SME_EXPORT LockPiFutex(std::atomic<FutexValueType>& var,
                            const std::chrono::milliseconds& timeout =
                                std::chrono::milliseconds::max()) -> PiFutexResult;
auto SME_EXPORT LockPiFutex(FutexValueType& value,
                            const std::chrono::milliseconds& timeout = kInfiniteTimeout)
    -> PiFutexResult;

void SME_EXPORT SetConsistentPiFutex(std::atomic<FutexValueType>& var) noexcept;
void SME_EXPORT SetConsistentPiFutexByResult(FutexValueType& value) noexcept;
auto SME_EXPORT IsPiFutexOwnerDied(FutexValueType value) noexcept -> bool;

auto SME_EXPORT HasPiFutexWaiters(FutexValueType value) noexcept -> bool;
void SME_EXPORT UnlockPiFutex(std::atomic<FutexValueType>& var);
void SME_EXPORT UnlockPiFutex(FutexValueType& value);

}  // namespace sme

#endif  // SME_INTERNAL_FUTEX_H
