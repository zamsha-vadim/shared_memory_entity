#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <csignal>
#include <deque>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "sme/adapt_spin_lock.h"
#include "sme/mapped_obj.h"
#include "test/shm_test_util.h"

const auto g_cpu_number = get_nprocs();

enum class ActionType : uint8_t { kWait, kReady, kExit };

struct WorkStruct {
    sme::AdaptiveSpinLock spin_lock;//{sme::AdaptiveSpinLock::Type::kSpinOnly};
    std::atomic<ActionType> ready{ActionType::kWait};
};

void Payload(uint64_t try_count, uint64_t item_count, sme::AdaptiveSpinLock& spin_lock)
{
    try {
        for (auto i = 0UL; i < try_count; i++) {
            std::deque<std::string> data;

            std::unique_lock ul{spin_lock};

            auto begin = std::numeric_limits<uint64_t>::max();
            auto end = begin - item_count;

            for (auto i = begin; i > end; i--) {
                std::string s = std::to_string(i);
                data.push_back(s);
            }

            std::sort(data.begin(), data.end());

            ul.unlock();
        }

    } catch (const std::exception& ex) {
        auto tid = gettid();

        std::string err_msg =
            "\n###ERROR:" + std::to_string(tid) + ": " + ex.what() + "\n";
        std::cout << std::flush << err_msg << std::flush;

        exit(EXIT_FAILURE);
    }
}

TEST(AdaptiveSpinLockTest, TestNotContendedLockAndUnlock)
{
    sme::AdaptiveSpinLock spin_lock;

    std::unique_lock ul{spin_lock, std::defer_lock};

    ul.lock();
}

TEST(AdaptiveSpinLockTest, TestThreadContended)
{
    sme::AdaptiveSpinLock::Type spin_type = sme::AdaptiveSpinLock::Type::kAdaptive;
    //sme::AdaptiveSpinLock::Type spin_type = sme::AdaptiveSpinLock::Type::kSpinOnly;

    sme::AdaptiveSpinLock spin_lock{spin_type};

    std::atomic<bool> ready{false};
    std::vector<std::thread> thrs;

    auto thr_func = [&ready, &spin_lock](uint64_t try_count, uint64_t item_count) {
        while (!ready) {
            ;
        }

        Payload(try_count, item_count, spin_lock);
    };

    //uint64_t try_count = 1000;
    uint64_t try_count = 20'000;
    //uint64_t try_count = 100'000UL;
    //uint64_t try_count = 1'000'000UL;

    uint64_t item_count = 50;

    auto thr_count = (g_cpu_number >= 8) ? (g_cpu_number - 2) : g_cpu_number;

    for (int i = 0; i < thr_count; i++)
        thrs.emplace_back(std::thread{thr_func, try_count, item_count});

    ready = true;

    for (auto& thr : thrs)
        thr.join();
}

TEST(AdaptiveSpinLockTest, TestProcessContended)
{
    auto shm_file = CreateTestSharedMemoryFile(GetUniqueName(), sizeof(WorkStruct));
    SharedMemoryFileDeleter smfd{shm_file};
    auto shm = shm_file.MapMemory(sme::kAllMemoryMapRequestForShared);

    auto* work_obj = sme::Construct<WorkStruct>(shm, 0);

    std::deque<pid_t> procs;

    auto proc_count = (g_cpu_number >= 8) ? (g_cpu_number - 2) : g_cpu_number;

    // uint64_t try_count = 1000;
    uint64_t try_count = 10'000;
    //uint64_t try_count = 100'000UL;
    // uint64_t try_count = 1'000'000UL;

    uint64_t item_count = 20;

    for (int i = 0; i < proc_count; i++) {
        auto pid = fork();

        if (pid > 0) {
            procs.push_back(pid);
        } else if (pid == 0) {
            while (work_obj->ready == ActionType::kWait) {
                ;
            }

            if (work_obj->ready == ActionType::kReady)
                Payload(try_count, item_count, work_obj->spin_lock);

            exit(EXIT_SUCCESS);

        } else {
            work_obj->ready = ActionType::kExit;
            break;
        }
    }

    bool failed{work_obj->ready == ActionType::kExit};

    if (work_obj->ready == ActionType::kWait)
        work_obj->ready = ActionType::kReady;

    for (auto pid : procs) {
        int proc_status{0};

        if (waitpid(pid, &proc_status, 0) != -1) {
            if (!WIFEXITED(proc_status))
                failed = true;
        } else {
            failed = true;
        }
    }

    work_obj->~WorkStruct();

    ASSERT_FALSE(failed);
}

TEST(AdaptiveSpinLockTest, TestRestoreAfterOneUnlockedProcess)
{
    auto shm_file = CreateTestSharedMemoryFile(GetUniqueName(), sizeof(sme::AdaptiveSpinLock));
    SharedMemoryFileDeleter smfd{shm_file};
    auto shm = shm_file.MapMemory(sme::kAllMemoryMapRequestForShared);

    auto* spin_lock = sme::Construct<sme::AdaptiveSpinLock>(shm, 0);

    auto pid = fork();
    if (pid == 0) {
        spin_lock->lock();
        exit(EXIT_SUCCESS);
    } else {
        ASSERT_TRUE(pid != -1);
    }

    int proc_status{0};
    auto res = waitpid(pid, &proc_status, 0);
    ASSERT_TRUE(res != -1);

    ASSERT_NO_THROW(spin_lock->lock());
    ASSERT_NO_THROW(spin_lock->unlock());

    spin_lock->~AdaptiveSpinLock();
}

TEST(AdaptiveSpinLockTest, TestRestoreAfterManyUnlockedProcesses)
{
    auto shm_file = CreateTestSharedMemoryFile(GetUniqueName(), sizeof(WorkStruct));
    SharedMemoryFileDeleter smfd{shm_file};
    auto shm = shm_file.MapMemory(sme::kAllMemoryMapRequestForShared);

    auto* work_obj = sme::Construct<WorkStruct>(shm, 0);

    std::deque<pid_t> procs;

    auto proc_count = (g_cpu_number >= 8) ? (g_cpu_number - 2) : g_cpu_number;

    // uint64_t try_count = 1000;
    uint64_t try_count = 10'000;
    //uint64_t try_count = 100'000UL;
    // uint64_t try_count = 1'000'000UL;

    uint64_t item_count = 200;

    for (int i = 0; i < proc_count; i++) {
        auto pid = fork();

        if (pid > 0) {
            procs.push_back(pid);
        } else if (pid == 0) {
            while (work_obj->ready == ActionType::kWait) {
                ;
            }

            if (work_obj->ready == ActionType::kReady) {
                if ((i % 2) != 0) {
                    Payload(try_count, item_count, work_obj->spin_lock);
                } else {
                    work_obj->spin_lock.lock();
                }
            }

            exit(EXIT_SUCCESS);

        } else {
            work_obj->ready = ActionType::kExit;
            break;
        }
    }

    bool failed{work_obj->ready == ActionType::kExit};

    if (work_obj->ready == ActionType::kWait)
        work_obj->ready = ActionType::kReady;

    for (auto pid : procs) {
        int proc_status{0};

        if (waitpid(pid, &proc_status, 0) != -1) {
            if (!WIFEXITED(proc_status))
                failed = true;
        } else {
            failed = true;
        }
    }

    work_obj->~WorkStruct();

    ASSERT_FALSE(failed);
}
