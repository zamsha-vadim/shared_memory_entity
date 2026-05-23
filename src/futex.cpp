#include "sme/futex.h"

#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include <system_error>
#include <utility>

// NOLINTBEGIN(icppcoreguidelines-pro-type-vararg, google-runtime-int,
// cppcoreguidelines-pro-type-reinterpret-cast)

namespace sme {

namespace {

auto Futex(FutexValueType* uaddr,
           int futex_op,
           FutexValueType val,
           const struct timespec* timeout,
           FutexValueType* uaddr2,
           FutexValueType val3) noexcept -> long
{
    return TEMP_FAILURE_RETRY(
        ::syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3));
}

auto WaitFutex(FutexValueType* addr, FutexValueType check_value, const struct timespec* timeout)
    -> long
{
    return Futex(addr, FUTEX_WAIT, check_value, timeout, nullptr, 0);
}

auto WakeFutex(FutexValueType* addr, FutexValueType waiter_number) noexcept -> long
{
    return Futex(addr, FUTEX_WAKE, waiter_number, nullptr, nullptr, 0);
}

auto ConvertTimeSpan(const std::chrono::milliseconds& ms_timeout) noexcept
    -> std::pair<struct timespec, bool>
{
    if (ms_timeout == std::chrono::milliseconds::zero())
        return std::make_pair(timespec{.tv_sec = 0, .tv_nsec = 0}, true);

    if (ms_timeout != std::chrono::milliseconds::max()) {
        struct timespec timeout {};

        timeout.tv_sec =
            std::chrono::duration_cast<std::chrono::seconds>(ms_timeout).count();

        timeout.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              ms_timeout % std::chrono::seconds(1))
                              .count();

        return std::make_pair(timeout, true);
    }

    return std::make_pair(timespec{.tv_sec = 0, .tv_nsec = 0}, false);
}

auto GetAddress(std::atomic<FutexValueType>& addr) noexcept -> FutexValueType*
{
    return reinterpret_cast<FutexValueType*>(&addr);
}

}  // namespace

auto WaitFutex(std::atomic<FutexValueType>& addr,
               FutexValueType check_value,
               const std::chrono::milliseconds& timeout) -> FutexResult
{
    auto [abs_time, time_limited] = ConvertTimeSpan(timeout);

    auto res =
        WaitFutex(GetAddress(addr), check_value, time_limited ? &abs_time : nullptr);
    if (res == 0)
        return FutexResult::kCompleted;

    switch (errno) {
    case 0:
        return FutexResult::kCompleted;
    case EAGAIN:
        return FutexResult::kCompleted;
    case ETIMEDOUT:
        return FutexResult::kTimeout;
    default:
        throw std::system_error(errno, std::generic_category(), "Futex wait error");
    }
}

auto WakeFutex(std::atomic<FutexValueType>& addr, FutexValueType waiter_number) -> uint32_t
{
    auto res = WakeFutex(GetAddress(addr), waiter_number);
    if (res == -1)
        throw std::system_error(errno, std::generic_category(), "Futex wake error");

    return static_cast<uint32_t>(res);
}

auto LockPiFutex(std::atomic<FutexValueType>& addr, const std::chrono::milliseconds& timeout)
    -> PiFutexResult
{
    auto [abs_time, time_limited] = ConvertTimeSpan(timeout);

    auto res = Futex(GetAddress(addr), FUTEX_LOCK_PI, 0,
                     (time_limited ? &abs_time : nullptr), nullptr, 0);
    if (res == 0)
        return PiFutexResult::kCompleted;

    switch (errno) {
    case 0:
        return PiFutexResult::kCompleted;
    case EAGAIN:
        return PiFutexResult::kOwnerDied;
    case ETIMEDOUT:
        return PiFutexResult::kTimeout;
    default:
        throw std::system_error(errno, std::generic_category(), "PI futex lock error");
    }
}

void UnlockPiFutex(std::atomic<FutexValueType>& addr)
{
    auto res = Futex(GetAddress(addr), FUTEX_UNLOCK_PI, 0, nullptr, nullptr, 0);
    if (res != 0)
        throw std::system_error(errno, std::generic_category(), "PI futex unlock error");
}

void SetConsistentPiFutex(std::atomic<FutexValueType>& addr) noexcept {
    auto owner_tid = addr.load(std::memory_order_acquire);

    if (!IsPiFutexOwnerDied(owner_tid))
        return;

    addr.compare_exchange_strong(owner_tid, 0, std::memory_order_release);
}

auto IsPiFutexOwnerDied(FutexValueType value) noexcept -> bool
{
    return ((value & FUTEX_OWNER_DIED) == FUTEX_OWNER_DIED);
}

auto HasPiFutexWaiters(FutexValueType value) noexcept -> bool
{
    return ((value & FUTEX_WAITERS) == FUTEX_WAITERS);
}

}  // namespace sme

// NOLINTEND(icppcoreguidelines-pro-type-vararg, google-runtime-int,
// cppcoreguidelines-pro-type-reinterpret-cast)
