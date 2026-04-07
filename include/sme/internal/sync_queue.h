#ifndef SME_INTERNAL_SYNC_QUEUE_H
#define SME_INTERNAL_SYNC_QUEUE_H

#include <variant>

#include "sme/internal/queue.h"
#include "sme/internal/spin_lock.h"
#include "sme/sync_type.h"

namespace sme {

template <typename T>
class SynchronizedQueue final {
   public:
    using ItemType = typename Queue<T>::ItemType;
    using ItemPointer = typename Queue<T>::ItemPointer;

   public:
    SynchronizedQueue(SyncronizationType sync_type);
    ~SynchronizedQueue();

    [[nodiscard]] auto Read() -> ItemPointer;
    void Write(ItemPointer obj);

    [[nodiscard]] auto WaitForReadAvailability() -> bool;

    auto IsEmpty() const -> bool;

   private:
    mutable std::variant<SpinLock> sync_;
    Queue<T> impl_;
};

}  // namespace sme

#include "sme/internal/sync_queue.tcc"

#endif  // SME_INTERNAL_SYNC_QUEUE_H
