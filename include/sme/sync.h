#ifndef SME_SYNC_H
#define SME_SYNC_H

#include <cstdint>
#include <variant>

#include "sme/adapt_spin_lock.h"
#include "sme/mutex.h"
#include "sme/sme_export.h"

namespace sme {

enum class SynchronizationType : uint8_t {
    kNone,
    kPrivateMutex,
    kSharedMutex,
    kAdaptiveSpinlock
};

class SME_EXPORT Synchronizer {
   public:
    explicit Synchronizer(SynchronizationType type);

    void lock();
    void unlock();

   private:
    SynchronizationType type_{};
    std::variant<std::monostate, Mutex, AdaptiveSpinLock> impl_;
};

}  // namespace sme

#endif  // SME_SYNC_H
