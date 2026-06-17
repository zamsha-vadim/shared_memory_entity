#ifndef SME_SYNC_H
#define SME_SYNC_H

#include <cstdint>
#include <variant>

#include "sme/adapt_spin_lock.h"
#include "sme/mutex.h"
#include "sme/sme_export.h"

namespace sme {

enum class SME_EXPORT SynchronizationType : uint8_t {
    kNone,
    kPrivateMutex,
    kSharedMutex,
    kAdaptiveSpinlock
};

class SME_EXPORT Synchronizer {
   public:
    explicit Synchronizer(SynchronizationType type);

    auto GetType() const noexcept -> SynchronizationType;

    void lock();
    void unlock();

   private:
    std::variant<AdaptiveSpinLock, Mutex, std::monostate> impl_;
    SynchronizationType type_{};
};

}  // namespace sme

#endif  // SME_SYNC_H
