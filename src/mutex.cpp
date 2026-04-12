#include "sme/mutex.h"

#include <cassert>
#include <system_error>

namespace sme {

namespace {

void SetAttributes(pthread_mutexattr_t& attr, InterprocessVisibility visibility)
{
    pthread_mutexattr_init(&attr);

    if (visibility != InterprocessVisibility::kShared) 
        return;

    auto err_code = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't set process-shared mutex attribute");

    err_code = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't set robustness mutex attribute");
}

void SetMutexConsistent(pthread_mutex_t& mutex)
{
    auto err_code = pthread_mutex_consistent(&mutex);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't repair mutex lock state consistent");

    err_code = pthread_mutex_unlock(&mutex);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't relock mutex after consistent state restoring");
}

}  // namespace

Mutex::Mutex(InterprocessVisibility visibility) : visibility_{visibility}
{
    pthread_mutexattr_t attr{};

    SetAttributes(attr, visibility_);

    auto err_code = pthread_mutex_init(&mutex_, &attr);

    pthread_mutexattr_destroy(&attr);

    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(), "Can't create mutex");
}

Mutex::~Mutex()
{
    [[maybe_unused]] int err_code{};

    assert(locked_ == 0);
    if (locked_) {
        err_code = pthread_mutex_unlock(&mutex_);
        assert(err_code == 0);
    }

    err_code = pthread_mutex_destroy(&mutex_);
    assert(err_code == 0);
}

auto Mutex::GetNativeObject() noexcept -> pthread_mutex_t&
{
    return mutex_;
}

void Mutex::lock()
{
    auto err_code = pthread_mutex_lock(&mutex_);
    if (err_code == 0) {
        ++locked_;
        return;
    }
    if (err_code != EOWNERDEAD) 
        throw std::system_error(err_code, std::generic_category(), "Can't lock mutex");

    SetMutexConsistent(mutex_);
    --locked_;

    err_code = pthread_mutex_lock(&mutex_);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't lock mutex after consistent state restoring");
    ++locked_;
}

auto Mutex::try_lock() -> bool
{
    auto err_code = pthread_mutex_trylock(&mutex_);
    if (err_code == 0) {
        ++locked_;
        return true;
    } 
    if (err_code == EBUSY) 
        return false;
    if (err_code != EOWNERDEAD)
        throw std::system_error(err_code, std::generic_category(), "Can't lock mutex");

    SetMutexConsistent(mutex_);
    --locked_;

    err_code = pthread_mutex_trylock(&mutex_);
    if (err_code == 0) {
        ++locked_;
        return true;
    } 
    if (err_code == EBUSY)
        return false;

    throw std::system_error(err_code, std::generic_category(),
                            "Can't lock mutex after consistent state restoring");
}

void Mutex::unlock()
{
    auto err_code = pthread_mutex_unlock(&mutex_);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(), "Can't unlock mutex");

    --locked_;
}

auto Mutex::GetVisibility() const noexcept -> InterprocessVisibility
{
    return visibility_;
}

}  // namespace sme
