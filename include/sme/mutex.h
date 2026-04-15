#ifndef SME_MUTEX_H
#define SME_MUTEX_H

#include <pthread.h>

#include <cstdint>
#include <atomic>

#include "sme/ipvt.h"
#include "sme/sme_export.h"

namespace sme {

class ConditionVariable;

class SME_EXPORT Mutex {
    friend class ConditionVariable;

   public:
    explicit Mutex(InterprocessVisibility visibility = InterprocessVisibility::kPrivate);

    Mutex(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    auto operator=(const Mutex&) -> Mutex& = delete;
    auto operator=(Mutex&&) -> Mutex& = delete;
    ~Mutex();

    void lock();
    auto try_lock() -> bool;
    void unlock();

    auto GetVisibility() const noexcept -> InterprocessVisibility;

   private:
    auto GetNativeObject() noexcept -> pthread_mutex_t&;

   private:
    pthread_mutex_t mutex_{};

    InterprocessVisibility visibility_{};
    std::atomic<uint64_t> locked_{0};
};

}  // namespace sme

#endif  // SME_MUTEX_H
