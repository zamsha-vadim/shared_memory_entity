#ifndef SME_SYNC_H
#define SME_SYNC_H

#include <cstdint>
#include <optional>

#include "sme/mutex.h"
#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT Synchronizer {
   public:
    enum class Type : uint8_t { kNone, kPrivate, kShared };

   public:
    explicit Synchronizer(Type type);

    void lock();
    auto try_lock() -> bool;
    void unlock();

   private:
    std::optional<Mutex> mutex_;
};

}  // namespace sme

#endif  // SME_SYNC_H

