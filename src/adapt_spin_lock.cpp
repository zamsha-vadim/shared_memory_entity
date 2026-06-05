#include "sme/adapt_spin_lock.h"

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <ctime>
#include <iostream>
#include <thread>

#include "sme/futex.h"
#include "sme/internal/util.h"

namespace sme {

namespace {

const unsigned int kCpuNumber{static_cast<unsigned int>(get_nprocs())};
constexpr uint64_t kNanosecondsPerSec{1'000'000'000UL};

[[maybe_unused]] auto ToNanoseconds(const timespec& time) noexcept -> uint64_t
{
    return time.tv_sec * kNanosecondsPerSec + time.tv_nsec;
}

[[maybe_unused]] auto GetCpuTime() noexcept -> uint64_t
{
    struct timespec time {};

    auto res = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    assert(res != -1);
    if (res == -1)
        return 0;

    return ToNanoseconds(time);
}

[[maybe_unused]] inline auto GetCpuCycles() noexcept -> uint64_t
{
#if defined(__x86_64__)
    unsigned int cpu;
    return __rdtscp(&cpu); 
#elif defined(__aarch64__)
    uint64_t counter;
    asm volatile(
        "isb\n\t"
        "mrs %0, cntvct_el0\n\t"
        : "=r"(counter)
        :
        : "memory");
    return counter << 6;
#else
    return 0;
#endif
}

auto GetTimestamp() noexcept -> uint64_t
{
#if defined(__x86_64__) || defined(__aarch64__)
    return GetCpuCycles();
#else
    return GetCpuTime();
#endif    
}

auto CalculateAverageTimeSpan(uint64_t begin_time,
                              uint64_t end_time,
                              uint64_t prev_avg_time_span) noexcept -> uint64_t
{
    auto curr_span = (end_time > begin_time) ? (end_time - begin_time) : 0;
    if (prev_avg_time_span == 0)
        prev_avg_time_span = curr_span;
    return (prev_avg_time_span * 15 + curr_span) / 16;
}

void LockFutex(pid_t curr_tid, std::atomic<FutexValueType>& sync_var)
{
    auto owner_tid = sync_var.load(std::memory_order_acquire);

    for (;;) {
        PiFutexResult res = LockPiFutex(sync_var);

        if (res == PiFutexResult::kCompleted) {
            if (IsPiFutexOwnerDied(sync_var))
                SetConsistentPiFutex(sync_var);
            return;
        }

        if (res == PiFutexResult::kOwnerDied) {
            if (sync_var.compare_exchange_strong(owner_tid, curr_tid,
                                                 std::memory_order_relaxed))
                return;
        }
    }
}

void UnlockFutex(std::atomic<FutexValueType>& sync_var)
{
    FutexValueType curr_tid = gettid();

    if (!sync_var.compare_exchange_strong(curr_tid, 0, std::memory_order_release))
        UnlockPiFutex(sync_var);
}

}  // namespace

AdaptiveSpinLock::AdaptiveSpinLock(Type type) : concur_wait_num_{kCpuNumber}, type_{type} {}

AdaptiveSpinLock::~AdaptiveSpinLock()
{
    assert(sync_var_.load() == 0);

    std::clog << "AVG EXEC TIME=" << avg_exec_time_ << ", AVG ACQ TIME=" << avg_acq_time_
              << ", MAX_ACQ_TIME=" << max_acq_time_ << ", LOCKS=" << lock_count_
              << ", RESCHED=" << resched_count_ << std::endl;
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
        if (type_ == Type::kAdaptive) {
            auto current_tail_id = this_lock_id + 1;

            begin_time = GetTimestamp();

            for (;;) {
                if (active_lock_id < this_lock_id) {
                    const auto max_wait_time =
                        CalculateWaitTime(active_lock_id, current_tail_id);

                    auto stage_elapsed_time = GetTimestamp() - begin_time;

                    if (stage_elapsed_time >= max_wait_time && max_wait_time != 0) {
                        LockFutex(curr_tid, sync_var_);

                        lock_count_.fetch_add(1, std::memory_order_relaxed);

                        auto curr_active_id = active_lock_id_.load(std::memory_order_relaxed);
                        while (curr_active_id < this_lock_id) {
                            if (active_lock_id_.compare_exchange_weak(
                                    curr_active_id, this_lock_id, std::memory_order_release,
                                    std::memory_order_relaxed))
                                break;
                        }

                        break;
                    }
                } else if (active_lock_id == this_lock_id) {
                    owner_tid = 0;
                    acquired = sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                                 std::memory_order_relaxed);
                    if (acquired)
                        break;
                } else {
                    resched_count_.fetch_add(1, std::memory_order_relaxed);

                    begin_time = GetTimestamp();
                    this_lock_id = last_lock_id_.fetch_add(1, std::memory_order_acq_rel);
                }

                RelaxCpu();

                active_lock_id = active_lock_id_.load(std::memory_order_relaxed);
                current_tail_id = last_lock_id_.load(std::memory_order_relaxed);
            }
        } else {
            for (;;) {
                if (active_lock_id == this_lock_id) {
                    owner_tid = 0;
                    acquired = sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                                 std::memory_order_relaxed);
                    if (acquired)
                        break;
                }
                RelaxCpu();

                active_lock_id = active_lock_id_.load(std::memory_order_relaxed);
            }
        }
    }

    UpdateAvarageAcquiringTime(begin_acq_time);
    MarkExecutionTimestamp();

    std::atomic_thread_fence(std::memory_order_acq_rel);
}

void AdaptiveSpinLock::unlock()
{
    if (type_ == Type::kAdaptive && concur_wait_num_ != 0)
        UpdateAvarageExecutionTime();

    UnlockFutex(sync_var_);
    active_lock_id_.fetch_add(1, std::memory_order_release);
}

auto AdaptiveSpinLock::CalculateWaitTime(uint64_t active_lock_id,
                                         uint64_t last_lock_id) noexcept -> uint64_t
{
    const auto avg_acq_time = avg_acq_time_.load(std::memory_order_relaxed);
    const auto avg_exec_time = avg_exec_time_.load(std::memory_order_relaxed);
    const auto avg_work_time =
        ((avg_exec_time > avg_acq_time) ? avg_exec_time : avg_acq_time);

    auto wait_multiplier = (last_lock_id != 0 && last_lock_id >= active_lock_id)
                               ? (last_lock_id - active_lock_id + 2)
                               : (concur_wait_num_ + 1);

    return (avg_work_time * wait_multiplier);
}

void AdaptiveSpinLock::UpdateAvarageAcquiringTime(uint64_t begin_acq_time) noexcept
{
    auto curr_time = GetTimestamp();

    auto last_acq_time = avg_acq_time_.load(std::memory_order_acquire);
    auto updated_acq_time =
        CalculateAverageTimeSpan(begin_acq_time, curr_time, last_acq_time);
    avg_acq_time_.store(updated_acq_time, std::memory_order_relaxed);

    auto curr_acq_time = curr_time - begin_acq_time;
    if (curr_acq_time > max_acq_time_.load(std::memory_order_relaxed))
        max_acq_time_.store(curr_acq_time, std::memory_order_relaxed);
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
