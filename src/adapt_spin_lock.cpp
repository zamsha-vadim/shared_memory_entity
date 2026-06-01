#include "sme/adapt_spin_lock.h"

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__x86_64__)
#include <immintrin.h>
#endif

#include <cassert>
#include <ctime>
#include <iostream>
#include <thread>

#include "sme/futex.h"

namespace sme {

namespace {

constexpr auto kNanosecondMax{1'000'000'000UL};

auto ToNanoseconds(const timespec& time) noexcept -> uint64_t
{
    return time.tv_sec * kNanosecondMax + time.tv_nsec;
}

auto GetThreadCpuTime() noexcept -> uint64_t
{
    struct timespec time {};

    //auto res = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
    auto res = clock_gettime(CLOCK_MONOTONIC, &time);
    assert(res != -1);
    if (res == -1)
        return 0;

    return ToNanoseconds(time);
}

inline auto GetCpuCycles() noexcept -> uint64_t
{
#if defined(__x86_64__)
    return __rdtsc();
#elif defined(__aarch64__)
    uint64_t virtual_timer_value;
    asm volatile(
        "isb\n\t"
        "mrs %0, cntvct_el0\n\t"
        : "=r"(virtual_timer_value)
        :
        : "memory");
    return virtual_timer_value;
#endif
}

auto GetTimestamp() noexcept -> uint64_t
{
    return GetCpuCycles();
    //return GetThreadCpuTime();
}

auto CalculateAverageTimeSpan(uint64_t begin_time,
                              uint64_t end_time,
                              uint64_t prev_avg_time_span) noexcept -> uint64_t
{
    auto curr_span = end_time - begin_time;
    if (prev_avg_time_span == 0)
        prev_avg_time_span = curr_span;
    return (prev_avg_time_span * 15 + curr_span) / 16;
}

void LockFutex(std::atomic<FutexValueType>& sync_var)
{
    auto res = LockPiFutex(sync_var);
    if (res == PiFutexResult::kOwnerDied)
        SetConsistentPiFutex(sync_var);
}

void UnlockFutex(std::atomic<FutexValueType>& sync_var)
{
    FutexValueType curr_tid = gettid();

    if (!sync_var.compare_exchange_strong(curr_tid, 0, std::memory_order_release))
        UnlockPiFutex(sync_var);
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

const unsigned int kCpuNumber{static_cast<unsigned int>(get_nprocs())};

}  // namespace

AdaptiveSpinLock::AdaptiveSpinLock() : concur_wait_num_{kCpuNumber} {}

AdaptiveSpinLock::~AdaptiveSpinLock()
{
    assert(sync_var_.load() == 0);

    std::cout << "AVG EXEC TIME=" << avg_exec_time_ << ", AVG ACQ TIME=" << avg_acq_time_
              << ", MAX_ACQ_TIME=" << max_acq_time_ << ", LOCKS=" << lock_count_
              << std::endl;
}

void AdaptiveSpinLock::lock()
{
    auto begin_acq_time = GetTimestamp();

    auto curr_tid = gettid();
    uint32_t owner_tid{0};

    bool acquired{false};
    uint64_t begin_time{0};

    auto active_lock_id = active_lock_id_.load(std::memory_order_acquire);
    auto this_lock_id = last_lock_id_.fetch_add(1, std::memory_order_relaxed);

    if (active_lock_id == this_lock_id)
        acquired = sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                     std::memory_order_relaxed);
    if (!acquired) {
        auto last_lock_id = this_lock_id + 1;
        begin_time = GetTimestamp();

        for (;;) {
            if (active_lock_id < this_lock_id) {
                const auto max_wait_time =
                    CalculateWaitTime(active_lock_id, last_lock_id);

                auto stage_elapsed_time = GetTimestamp() - begin_time;

                if (stage_elapsed_time >= max_wait_time && max_wait_time != 0) {
                    //continue;
                    //std::cout << stage_elapsed_time << ", " << max_wait_time << ", " << try_count << std::endl;
                    //std::cout << this_lock_id << std::endl;

                    lock_count_.fetch_add(1, std::memory_order_relaxed);

                    LockFutex(sync_var_);
                    acquired = true;

                    break;
                }
            } else if (active_lock_id == this_lock_id) {
                owner_tid = 0;
                acquired = sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                             std::memory_order_relaxed);
                if (acquired)
                    break;
            } else {
                //std::cout << "!!!!\n";
                //abort();
                lock_count_.fetch_add(1, std::memory_order_relaxed);

                LockFutex(sync_var_);
                acquired = true;

                break;
            }

            active_lock_id = active_lock_id_.load(std::memory_order_relaxed);
            last_lock_id = last_lock_id_.load(std::memory_order_relaxed);
        }
    }

    /*
    if (!acquired) {
        if (begin_time == 0)
            begin_time = GetTimestamp();

        for (;;) {
            owner_tid = sync_var_.load(std::memory_order_acquire);
            if (owner_tid == 0) {
                if (sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                      std::memory_order_relaxed))
                    break;
            }

            const auto curr_time = GetTimestamp();
            if (curr_time < begin_time)
                begin_time = curr_time;

            const auto stage_elapsed_time = curr_time - begin_time;
            const auto max_wait_time =
                avg_exec_time_.load(std::memory_order_relaxed) * concur_wait_num_;

            if (stage_elapsed_time >= max_wait_time && max_wait_time != 0) {
                // std::cout << "LLL E=" << elapsed_time << ", A=" <<
                // avg_exec_time_<< std::endl;
                lock_count_.fetch_add(1, std::memory_order_relaxed);

                LockFutex(sync_var_);
                break;
            }

            RelaxCpu();
        }
    }
    */

    UpdateAvarageAcquiringTime(begin_acq_time);
    MarkExecutionTimestamp();
}

void AdaptiveSpinLock::unlock()
{
    if (concur_wait_num_ != 0)
        UpdateAvarageExecutionTime();

    UnlockFutex(sync_var_);
    active_lock_id_.fetch_add(1, std::memory_order_relaxed);
}

auto AdaptiveSpinLock::CalculateWaitTime(uint64_t active_lock_id,
                                         uint64_t last_lock_id) noexcept -> uint64_t
{
    const auto avg_acq_time = avg_acq_time_.load(std::memory_order_relaxed);
    const auto avg_exec_time = avg_exec_time_.load(std::memory_order_relaxed);
    const auto avg_work_time =
        ((avg_exec_time > avg_acq_time_) ? avg_exec_time : avg_acq_time);

    auto wait_multiplier = (last_lock_id >= active_lock_id)
                                     ? (last_lock_id - active_lock_id + 2)
                                     : (concur_wait_num_ + 1);
    //wait_multiplier = concur_wait_num_;

    return (avg_work_time * wait_multiplier);
}

void AdaptiveSpinLock::UpdateAvarageAcquiringTime(uint64_t begin_acq_time) noexcept
{
    auto curr_time = GetTimestamp();

    auto last_acq_time = avg_acq_time_.load(std::memory_order_acquire);
    auto updated_acq_time =
        CalculateAverageTimeSpan(begin_acq_time, curr_time, last_acq_time);
    avg_acq_time_.store(updated_acq_time, std::memory_order_release);

    auto curr_acq_time = curr_time - begin_acq_time;
    if (curr_acq_time > max_acq_time_.load(std::memory_order_acquire))
        max_acq_time_.store(curr_acq_time, std::memory_order_release);
}

void AdaptiveSpinLock::MarkExecutionTimestamp() noexcept
{
    begin_timestamp_ = GetTimestamp();
}

void AdaptiveSpinLock::UpdateAvarageExecutionTime() noexcept
{
    auto last_avg_time = avg_exec_time_.load(std::memory_order_acquire);
    auto updated_avg_time =
        CalculateAverageTimeSpan(begin_timestamp_, GetTimestamp(), last_avg_time);

    avg_exec_time_.store(updated_avg_time, std::memory_order_release);
}

}  // namespace sme
