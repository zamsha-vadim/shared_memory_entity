#ifndef SME_COND_VAR_H

#include <pthread.h>

#include <chrono>
#include <mutex>

#include "sme/mutex.h"
#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT ConditionVariable {
   public:
    explicit ConditionVariable(
        InterprocessVisibility visibility = InterprocessVisibility::kPrivate);

    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable(ConditionVariable&&) = delete;
    auto operator=(const ConditionVariable&) -> ConditionVariable& = delete;
    auto operator=(ConditionVariable&&) -> ConditionVariable& = delete;

    ~ConditionVariable();

    void Wait(std::unique_lock<Mutex>& mutex_lock);

    template <class Rep, class Period>
    auto WaitFor(std::unique_lock<Mutex>& mutex_lock,
                 const std::chrono::duration<Rep, Period>& timeout) -> bool;

    void NotifyOne();
    void NotifyAll();

    auto GetVisibility() const noexcept -> InterprocessVisibility;

   private:
    auto WaitFor(std::unique_lock<Mutex>& mutex_lock, const struct timespec& abs_time)
        -> bool;

    template <class Rep, class Period>
    auto GetAbsoluteTime(const std::chrono::duration<Rep, Period>& timeout) const
        -> struct timespec;

   private:
    pthread_cond_t cond_var_{};
    InterprocessVisibility visibility_{};
};

template <class Rep, class Period>
auto ConditionVariable::WaitFor(std::unique_lock<Mutex>& mutex_lock,
                                const std::chrono::duration<Rep, Period>& timeout)
    -> bool {
    if (timeout == std::chrono::duration<Rep, Period>::max()) {
        Wait(mutex_lock);
        return true;
    }

    return WaitFor(mutex_lock, GetAbsoluteTime(timeout));
}

template <class Rep, class Period>
auto ConditionVariable::GetAbsoluteTime(
    const std::chrono::duration<Rep, Period>& timeout) const -> struct timespec {
    auto abs_time = std::chrono::system_clock::now() + timeout;
    auto epoch_ns_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(abs_time.time_since_epoch());

    struct timespec abs_ts {};

    abs_ts.tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(epoch_ns_time).count();
    abs_ts.tv_nsec = (epoch_ns_time % std::chrono::seconds(1)).count();

    return abs_ts;
}

}  // namespace sme

#endif // SME_COND_VAR_H
