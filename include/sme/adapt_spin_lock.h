#ifndef SME_ADAPT_SPIN_LOCK_H
#define SME_ADAPT_SPIN_LOCK_H

#include <atomic>
#include <cstdint>

#include "sme/internal/type.h"
#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT AdaptiveSpinLock final {
   public:
    enum class Type { kAdaptive, kSpinOnly };

   public:
    explicit AdaptiveSpinLock(Type type = Type::kAdaptive);

    AdaptiveSpinLock(const AdaptiveSpinLock&) = delete;
    AdaptiveSpinLock(AdaptiveSpinLock&&) = delete;
    auto operator=(const AdaptiveSpinLock&) -> AdaptiveSpinLock& = delete;
    auto operator=(AdaptiveSpinLock&&) -> AdaptiveSpinLock& = delete;

    ~AdaptiveSpinLock();

    void lock();
    void unlock();

   private:
    auto CalculateWaitTime(uint64_t active_lock_id, uint64_t last_lock_id) noexcept
        -> uint64_t;

    inline void UpdateAverageAcquiringTime(uint64_t begin_acq_time) noexcept;
    
    inline void MarkExecutionTimestamp() noexcept;
    inline void UpdateAverageExecutionTime() noexcept;

   private:
    alignas(kCacheLineSize) std::atomic<uint32_t> sync_var_{0};
    std::atomic<uint64_t> active_lock_id_{0};
    std::atomic<uint64_t> last_lock_id_{0};

    uint64_t begin_timestamp_{0};
    std::atomic<uint64_t> avg_exec_time_{0};
    std::atomic<uint64_t> avg_acq_time_{0};
    std::atomic<uint64_t> max_acq_time_{0};
    
    std::atomic<uint64_t> lock_count_{0};
    std::atomic<uint64_t> resched_count_{0};

    const unsigned int concur_wait_num_{};
    const Type type_{};
};

}  // namespace sme

#endif  // SME_ADAPT_SPIN_LOCK_H
