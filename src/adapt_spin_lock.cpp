#include "sme/adapt_spin_lock.h"

#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <ctime>
#include <thread>

#include "sme/futex.h"

namespace sme {

namespace {

constexpr auto kNanosecondMax{1'000'000'000UL};

auto ToNanoseconds(const timespec& time) noexcept -> uint64_t
{
    return time.tv_sec * kNanosecondMax + time.tv_nsec;
}

auto GetTimestamp() noexcept -> uint64_t
{
    struct timespec time {};

    auto res = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
    if (res == -1)
        return 0;

    return ToNanoseconds(time);
}

auto LockFutex(std::atomic<FutexValueType>& sync_var) -> bool
{
    auto res = LockPiFutex(sync_var);

    if (res == PiFutexResult::kOwnerDied) {
        SetConsistentPiFutex(sync_var);
        return false;
    }

    assert(res == PiFutexResult::kCompleted);
    return true;
}

void UnlockFutex(std::atomic<FutexValueType>& sync_var)
{
    auto value = sync_var.load(std::memory_order_acquire);
    if (!HasPiFutexWaiters(value))
        sync_var.store(0, std::memory_order_release);
    else
        UnlockPiFutex(sync_var);
}

void RelaxCpu()
{
#if defined(x86_64)
    __builtin_ia32_pause();
#elif defined(aarch64) 
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
}

void AdaptiveSpinLock::lock()
{
    auto curr_tid = gettid();
    uint32_t owner_tid{0};

    if (!sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                           std::memory_order_acq_rel)) {
        auto begin_time = GetTimestamp();

        owner_tid = 0;

        while (!sync_var_.compare_exchange_strong(owner_tid, curr_tid,
                                                  std::memory_order_relaxed)) {
            const auto elapsed_time = GetTimestamp() - begin_time;
            const auto max_wait_time =
                avg_exec_time_.load(std::memory_order_acquire) * concur_wait_num_;

            if (elapsed_time >= max_wait_time) {
                auto locked = LockFutex(sync_var_);
                if (locked)
                    break;

                begin_time = GetTimestamp();
            }

            owner_tid = 0;

            RelaxCpu();
        }
    }

    MarkExecutionTimestamp();
}

void AdaptiveSpinLock::unlock()
{
    UpdateAvarageExecutionTime();

    UnlockFutex(sync_var_);
}

void AdaptiveSpinLock::MarkExecutionTimestamp() noexcept
{
    begin_timestamp_ = GetTimestamp();
}

void AdaptiveSpinLock::UpdateAvarageExecutionTime() noexcept
{
    auto curr_exec_time = GetTimestamp() - begin_timestamp_;
    auto last_avg_exec_time = avg_exec_time_.load(std::memory_order_acquire);

    auto updated_avg_exec_time = (last_avg_exec_time * 15 + curr_exec_time) / 16;

    avg_exec_time_.store(updated_avg_exec_time, std::memory_order_release);
}

}  // namespace sme
