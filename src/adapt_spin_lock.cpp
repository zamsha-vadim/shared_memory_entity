#include "sme/adapt_spin_lock.h"

#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__x86_64__)
#include <x86intrin.h>
#endif

#include <cassert>
#include <ctime>
#include <iostream>
#include <thread>

#include "sme/futex.h"

namespace sme {

namespace {

const unsigned int kCpuNumber{static_cast<unsigned int>(get_nprocs())};

constexpr uint64_t kNanosecondsPerSec{1'000'000'000UL};
constexpr uint8_t kTscShift{32};

auto ToNanoseconds(const timespec& time) noexcept -> uint64_t
{
    return time.tv_sec * kNanosecondsPerSec + time.tv_nsec;
}

auto SubNanoseconds(const timespec& start, const timespec& end) noexcept -> uint64_t
{
    auto sec = end.tv_sec - start.tv_sec;
    uint64_t nsec = 0;

    if (end.tv_nsec >= start.tv_nsec) {
        nsec = end.tv_nsec - start.tv_nsec;
    } else {
        sec -= 1;
        nsec = (end.tv_nsec + kNanosecondsPerSec) - start.tv_nsec;
    }

    return (sec * kNanosecondsPerSec) + nsec;
}

inline auto GetCpuCycles() noexcept -> uint64_t
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
    return counter;
#endif
}

auto CalibrateTimer() noexcept -> uint64_t
{
    const struct timespec request_sleep_time {
        .tv_sec = 0, .tv_nsec = 10'000'000
    };
    struct timespec start_real{}, end_real{};
    struct timespec remaining_time{};

    uint64_t start_cycles = 0;
    uint64_t end_cycles = 0;

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_real);
        start_cycles = GetCpuCycles();

        int res = nanosleep(&request_sleep_time, &remaining_time);
        if (res == -1)
            continue;

        end_cycles = GetCpuCycles();
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_real);

        if (end_cycles > start_cycles)
            break;
    }

    uint64_t delta_ns = SubNanoseconds(start_real, end_real);
    uint64_t delta_cycles = end_cycles - start_cycles;

    if (delta_cycles > 0 && delta_ns > 0) {
        return (delta_ns << kTscShift) / delta_cycles;
    }

    return 0;
}

auto GetThreadCpuTime() noexcept -> uint64_t
{
    struct timespec time {};

    auto res = clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    assert(res != -1);
    if (res == -1)
        return 0;

    return ToNanoseconds(time);
}

const uint64_t kTscMultiplier = CalibrateTimer();

auto GetTimestamp() noexcept -> uint64_t
{
    // TODO
    return (kTscMultiplier == 0) ? ((GetCpuCycles() * kTscMultiplier) >> kTscShift)
                                 : GetThreadCpuTime();
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

void RelaxCpu()
{
#if defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#endif
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

}  // namespace

AdaptiveSpinLock::AdaptiveSpinLock(Type type) : concur_wait_num_{kCpuNumber}, type_{type} {}

AdaptiveSpinLock::~AdaptiveSpinLock()
{
    assert(sync_var_.load() == 0);

    std::cout << "AVG EXEC TIME=" << avg_exec_time_ << ", AVG ACQ TIME=" << avg_acq_time_
              << ", MAX_ACQ_TIME=" << max_acq_time_ << ", LOCKS=" << lock_count_ << ", RESCHED=" << resched_count_
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
    auto this_lock_id = last_lock_id_.fetch_add(1, std::memory_order_acq_rel);

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
                        LockFutex(sync_var_);

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
