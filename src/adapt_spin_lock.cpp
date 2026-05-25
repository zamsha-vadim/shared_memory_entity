#include "sme/adapt_spin_lock.h"

#include <sys/types.h>
#include <unistd.h>

#if defined(__x86_64__)
#include <immintrin.h>
#endif

#include <cassert>
#include <ctime>
#include <thread>
#include <iostream>

#include "sme/futex.h"

namespace sme {

namespace {

inline auto GetCpuCycles() noexcept -> uint64_t
{
#if defined(__x86_64__)
    return __rdtsc();
#elif defined(__aarch64__)
    uint64_t virtual_timer_value;
    asm volatile(
        "isb\n\t"
        "mrs %0, cntvct_el0\n\t"
        : "=r" (virtual_timer_value)
        :
        : "memory"
    );
    return virtual_timer_value;
#endif
}

auto GetTimestamp() noexcept -> uint64_t
{
    return GetCpuCycles();
}

void LockFutex(std::atomic<FutexValueType>& sync_var)
{
//    std::string s = std::to_string(gettid()) + ": LLLLLLLLLLLLLLLL\n";
//    std::cout << s << std::flush;

    auto res = LockPiFutex(sync_var);
    if (res == PiFutexResult::kOwnerDied)
        SetConsistentPiFutex(sync_var);
}

void UnlockFutex(std::atomic<FutexValueType>& sync_var)
{
    FutexValueType curr_tid = gettid();

    if (!sync_var.compare_exchange_strong(curr_tid, 0, std::memory_order_release)) {
//        std::string s = std::to_string(gettid()) + ": UUUUUUUUUUUUUUUU\n";
//        std::cout << s << std::flush;

        UnlockPiFutex(sync_var);
    }
}

void RelaxCpu()
{
#if defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) 
    asm volatile("yield" ::: "memory");  
#else
    std::this_thread::yield();
#endif
}

}  // namespace

AdaptiveSpinLock::AdaptiveSpinLock(unsigned short concur_wait_num)
    : concur_wait_num_{concur_wait_num}
{
}

AdaptiveSpinLock::~AdaptiveSpinLock()
{
    assert(sync_var_.load() == 0);

    std::cout << "AVG EXEC TIME=" << avg_exec_time_ << ", AVG ACQ TIME=" << avg_acq_time_
              << ", LOCKS=" << lock_count_ << std::endl;
}

void AdaptiveSpinLock::lock()
{
    if (concur_wait_num_ == 0) {
        LockFutex(sync_var_);
        return;
    }
    
    auto curr_tid = gettid();
    uint32_t owner_tid{0};

    auto begin_acq_time = GetTimestamp();

    if (!sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                           std::memory_order_relaxed)) {
        auto begin_time = GetTimestamp();

        for (;;) {
            const auto elapsed_time = GetTimestamp() - begin_time;
            const auto max_wait_time =
                avg_exec_time_.load(std::memory_order_relaxed) * concur_wait_num_;

            if (elapsed_time > max_wait_time && max_wait_time != 0) {
                //std::cout << "LLL E=" << elapsed_time << ", A=" << avg_exec_time_<< std::endl;

                lock_count_.fetch_add(1, std::memory_order_relaxed);

                LockFutex(sync_var_);
                break;
            }

            RelaxCpu();

            owner_tid = sync_var_.load(std::memory_order_acquire);
            if (owner_tid == 0) {
                if (sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                      std::memory_order_relaxed)) {
                    /*
                    if (elapsed_time > max_wait_time && max_wait_time != 0) {
                        std::cout << curr_tid << " E=" << elapsed_time
                                  << ", A=" << avg_exec_time_ << std::endl;
//                                  abort();
                    }
                    */

                    break;
                }
            }
        }
    }

    auto curr_acq_time = GetTimestamp() - begin_acq_time;
    auto last_acq_time = avg_acq_time_.load(std::memory_order_acquire);
    if (last_acq_time == 0)
        last_acq_time = curr_acq_time;
    auto updated_acq_time = (last_acq_time * 15 + curr_acq_time) / 16;
    avg_acq_time_.store(updated_acq_time, std::memory_order_release);

    MarkExecutionTimestamp();
}

void AdaptiveSpinLock::unlock()
{
    if (concur_wait_num_ != 0) 
        UpdateAvarageExecutionTime();
    // std::cout << "A: " << avg_exec_time_.load(std::memory_order_acquire) <<
    // std::endl;

    UnlockFutex(sync_var_);
}

void AdaptiveSpinLock::MarkExecutionTimestamp() noexcept
{
    begin_timestamp_ = GetTimestamp();
}

void AdaptiveSpinLock::UpdateAvarageExecutionTime() noexcept
{
    auto curr_exec_time = GetTimestamp() - begin_timestamp_;

    //std::cout << "C: " << curr_exec_time << std::endl;

    auto last_avg_time = avg_exec_time_.load(std::memory_order_acquire);
    if (last_avg_time == 0)
        last_avg_time = curr_exec_time;

    auto updated_avg_time = (last_avg_time * 15 + curr_exec_time) / 16;

    avg_exec_time_.store(updated_avg_time, std::memory_order_release);
}

}  // namespace sme
