#ifndef SME_ADAPT_SPIN_LOCK_H
#define SME_ADAPT_SPIN_LOCK_H

#include <atomic>
#include <cstdint>

#include "sme/internal/type.h"
#include "sme/sme_export.h"

namespace sme {

class alignas(kCacheLineSize) SME_EXPORT AdaptiveSpinLock {
   public:
    enum class Type { kAdaptive, kSpinOnly };

   public:
    AdaptiveSpinLock(Type type = Type::kAdaptive);

    AdaptiveSpinLock(const AdaptiveSpinLock&) = delete;
    AdaptiveSpinLock(AdaptiveSpinLock&&) = delete;
    auto operator=(const AdaptiveSpinLock&) -> AdaptiveSpinLock& = delete;
    auto operator=(AdaptiveSpinLock&&) -> AdaptiveSpinLock& = delete;

    ~AdaptiveSpinLock();

    void lock();
    //    auto try_lock() -> bool;
    void unlock();

   private:
    auto CalculateWaitTime(uint64_t active_lock_id, uint64_t last_lock_id) noexcept
        -> uint64_t;

    inline void UpdateAvarageAcquiringTime(uint64_t begin_acq_time) noexcept;
    
    inline void MarkExecutionTimestamp() noexcept;
    inline void UpdateAvarageExecutionTime() noexcept;

   private:
    uint64_t begin_timestamp_{0};
    std::atomic<uint64_t> avg_exec_time_{0};
    std::atomic<uint64_t> avg_acq_time_{0};
    std::atomic<uint64_t> max_acq_time_{0};
    
    alignas(kCacheLineSize) std::atomic<uint32_t> sync_var_{0};
    std::atomic<uint64_t> active_lock_id_{0};
    std::atomic<uint64_t> last_lock_id_{0};

    std::atomic<uint64_t> lock_count_{0};
    std::atomic<uint64_t> resched_count_{0};
    unsigned int concur_wait_num_{};
    Type type_{};
};

}  // namespace sme

#endif  // SME_ADAPT_SPIN_LOCK_H
