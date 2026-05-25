#ifndef SME_ADAPT_SPIN_LOCK_H
#define SME_ADAPT_SPIN_LOCK_H

#include <atomic>
#include <cstdint>

#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT AdaptiveSpinLock {
   public:
    AdaptiveSpinLock(unsigned short concur_wait_num = 4);

    AdaptiveSpinLock(const AdaptiveSpinLock&) = delete;
    AdaptiveSpinLock(AdaptiveSpinLock&&) = delete;
    auto operator=(const AdaptiveSpinLock&) -> AdaptiveSpinLock& = delete;
    auto operator=(AdaptiveSpinLock&&) -> AdaptiveSpinLock& = delete;

    ~AdaptiveSpinLock();

    void lock();
    //    auto try_lock() -> bool;
    void unlock();

   private:
    void MarkExecutionTimestamp() noexcept;
    void UpdateAvarageExecutionTime() noexcept;

   private:
    unsigned short concur_wait_num_{};
    uint64_t begin_timestamp_{0};
    std::atomic<uint64_t> avg_exec_time_{0};
    std::atomic<uint64_t> avg_acq_time_{0};
    std::atomic<uint64_t> lock_count_{0};
    std::atomic<uint32_t> sync_var_{0};
};

}  // namespace sme

#endif  // SME_ADAPT_SPIN_LOCK_H
