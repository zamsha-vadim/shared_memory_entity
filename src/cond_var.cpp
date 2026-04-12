#include "sme/cond_var.h"

#include <unistd.h>

#include <cassert>
#include <system_error>

namespace sme {

namespace {

void SetAttributes(pthread_condattr_t& attr, InterprocessVisibility visibility)
{
    pthread_condattr_init(&attr);

    int err_code{};

    if (visibility != InterprocessVisibility::kShared) 
        return;

    err_code = pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't set process-shared condition variable attribute");
}

void ValidateMutex(const std::unique_lock<Mutex>& mutex_lock)
{
    if (!mutex_lock.owns_lock())
        throw std::logic_error("Mutex must be locked");

    auto* mutex = mutex_lock.mutex();
    if (mutex == nullptr)
        throw std::logic_error("No associated mutex");
}

}  // namespace

ConditionVariable::ConditionVariable(InterprocessVisibility visibility)
    : visibility_{visibility}
{
    pthread_condattr_t attr{};

    SetAttributes(attr, GetVisibility());

    auto err_code = pthread_cond_init(&cond_var_, &attr);

    pthread_condattr_destroy(&attr);

    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't create condition variable");
}

ConditionVariable::~ConditionVariable()
{
    [[maybe_unused]] auto err_code = pthread_cond_destroy(&cond_var_);
    assert(err_code == 0);
}

void ConditionVariable::Wait(std::unique_lock<Mutex>& mutex_lock) {
    ValidateMutex(mutex_lock);

    auto* mutex = mutex_lock.mutex();

    auto err_code =
        TEMP_FAILURE_RETRY(pthread_cond_wait(&cond_var_, &mutex->GetNativeObject()));
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't wait on condition variable");
}

auto ConditionVariable::WaitFor(std::unique_lock<Mutex>& mutex_lock,
                                const struct timespec& abs_time) -> bool
{
    ValidateMutex(mutex_lock);

    auto* mutex = mutex_lock.mutex();

    auto err_code = TEMP_FAILURE_RETRY(
        pthread_cond_timedwait(&cond_var_, &mutex->GetNativeObject(), &abs_time));

    if (err_code == 0)
        return true;
    if (err_code == ETIMEDOUT)
        return false;

    throw std::system_error(err_code, std::generic_category(),
                            "Can't wait on condition variable");
}

void ConditionVariable::NotifyOne() {
    auto err_code = pthread_cond_signal(&cond_var_);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't notify one waiter of condition variable");
}

void ConditionVariable::NotifyAll()
{
    auto err_code = pthread_cond_broadcast(&cond_var_);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't notify waiters of condition variable");
}

auto ConditionVariable::GetVisibility() const noexcept -> InterprocessVisibility
{
    return visibility_;
}

}  // namespace sme
