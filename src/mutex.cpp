#include "sme/mutex.h"

#include <cassert>
#include <system_error>

namespace sme {

namespace {

void SetMutexAttributes(pthread_mutexattr_t& attr, Mutex::Type type)
{
    pthread_mutexattr_init(&attr);

    int err_code{};

    if (type == Mutex::Type::kShared) {
        err_code = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        if (err_code != 0)
            throw std::system_error(err_code, std::generic_category(),
                                    "Can't set process-shared mutex attribute");

        err_code = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        if (err_code != 0)
            throw std::system_error(err_code, std::generic_category(),
                                    "Can't set robustness mutex attribute");
    }
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

auto CreateOptionalMutex(Synchronizer::Type sync_type) -> std::optional<Mutex>
{
    switch (sync_type) {
        case Synchronizer::Type::kNone:
            return {};

        case Synchronizer::Type::kPrivate:
            return std::optional<Mutex>{std::in_place, Mutex::Type::kPrivate};

        case Synchronizer::Type::kShared:
            return std::optional<Mutex>{std::in_place, Mutex::Type::kShared};

        default:
            throw std::logic_error("Not supported synchronization type");
    }
}

}  // namespace

// Mutex implementation

Mutex::Mutex(Type type)
{
    pthread_mutexattr_t attr{};

    SetMutexAttributes(attr, type);

    auto err_code = pthread_mutex_init(&mutex_, &attr);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(), "Can't create mutex");
}

Mutex::~Mutex()
{
    [[maybe_unused]] int err_code{};

    if (locked_) {
        err_code = pthread_mutex_unlock(&mutex_);
        assert(err_code == 0);
    }

    err_code = pthread_mutex_destroy(&mutex_);
    assert(err_code == 0);
}

void Mutex::lock()
{
    auto err_code = pthread_mutex_lock(&mutex_);
    if (err_code == 0) {
        locked_ = true;
        return;
    }
    if (err_code != EOWNERDEAD) {
        throw std::system_error(err_code, std::generic_category(), "Can't lock mutex");
    }

    SetMutexConsistent(mutex_);
    locked_ = false;

    err_code = pthread_mutex_lock(&mutex_);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(),
                                "Can't lock mutex after consistent state restoring");
    locked_ = true;
}

auto Mutex::try_lock() -> bool
{
    auto err_code = pthread_mutex_trylock(&mutex_);
    if (err_code == 0) {
        locked_ = true;
        return true;
    } 
    if (err_code == EBUSY) 
        return false;
    if (err_code != EOWNERDEAD)
        throw std::system_error(err_code, std::generic_category(), "Can't lock mutex");

    SetMutexConsistent(mutex_);
    locked_ = false;

    err_code = pthread_mutex_trylock(&mutex_);
    if (err_code == 0) {
        locked_ = true;
        return true;
    } 
    if (err_code == EBUSY)
        return false;

    throw std::system_error(err_code, std::generic_category(),
                            "Can't lock mutex after consistent state restoring");
}

void Mutex::unlock()
{
    if (!locked_)
        return;

    auto err_code = pthread_mutex_unlock(&mutex_);
    if (err_code != 0)
        throw std::system_error(err_code, std::generic_category(), "Can't unlock mutex");

    locked_ = false;
}

// Synchronizer implementation

Synchronizer::Synchronizer(Type type) : mutex_{CreateOptionalMutex(type)} {}

void Synchronizer::lock()
{
    if (mutex_)
        mutex_->lock();
}

auto Synchronizer::try_lock() -> bool
{
    return (mutex_ ? mutex_->try_lock() : true);
}

void Synchronizer::unlock()
{
    if (mutex_)
        mutex_->unlock();
}

}  // namespace sme
