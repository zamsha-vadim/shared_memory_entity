#include "sme/internal/futex.h"

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

auto Futex(uint32_t* uaddr,
           int futex_op,
           uint32_t val,
           const struct timespec* timeout,
           uint32_t* uaddr2,
           uint32_t val3) noexcept -> long
{
    return TEMP_FAILURE_RETRY(
        ::syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3));
}

auto FutexWait(uint32_t* addr, uint32_t check_value, const struct timespec* timeout)
    -> long
{
    return Futex(addr, FUTEX_WAIT, check_value, timeout, nullptr, 0);
}

auto FutexWake(uint32_t* addr, uint32_t waiter_number) noexcept -> long
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

}  // namespace

auto FutexWait(std::atomic<uint32_t>* addr,
               uint32_t check_value,
               const std::chrono::milliseconds& ms_timeout) -> FutexResult
{
    auto [timeout, time_limited] = ConvertTimeSpan(ms_timeout);

    auto res = FutexWait(reinterpret_cast<uint32_t*>(addr), check_value,
                         time_limited ? &timeout : nullptr);
    if (res == 0)
        return FutexResult::kCompleted;

    if (errno == EAGAIN || res == EWOULDBLOCK)
        return FutexResult::kCompleted;
    else if (errno == ETIMEDOUT)
        return FutexResult::kTimeout;
    else
        throw std::system_error(errno, std::generic_category(), "Futex wait error");
}

auto SME_EXPORT FutexWake(std::atomic<uint32_t>* addr, uint32_t waiter_number) -> uint32_t
{
    auto res = FutexWake(reinterpret_cast<uint32_t*>(addr), waiter_number);
    if (res == -1)
        throw std::system_error(errno, std::generic_category(), "Futex wake error");

    return static_cast<uint32_t>(res);
}

}  // namespace sme

// NOLINTEND(icppcoreguidelines-pro-type-vararg, google-runtime-int,
// cppcoreguidelines-pro-type-reinterpret-cast)
