#ifndef SME_MUTEX_H
#define SME_MUTEX_H

#include <pthread.h>

#include <cstdint>
#include <optional>

#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT Mutex {
   public:
    enum class Type : uint8_t { kPrivate, kShared };

   public:
    explicit Mutex(Type type = Type::kPrivate);

    Mutex(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    auto operator=(const Mutex&) -> Mutex& = delete;
    auto operator=(Mutex&&) -> Mutex& = delete;
    ~Mutex();

    void lock();
    auto try_lock() -> bool;
    void unlock();

   private:
    pthread_mutex_t mutex_{};
    bool locked_{false};
};

class Synchronizer {
   public:
    enum class Type : uint8_t { kNone, kPrivate, kShared };

   public:
    Synchronizer(Type type);

    void lock();
    auto try_lock() -> bool;
    void unlock();

   private:
    std::optional<Mutex> mutex_;
};

}  // namespace sme

#endif  // SME_MUTEX_H
