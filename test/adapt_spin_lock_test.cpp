#include <climits>
#include <list>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "sme/adapt_spin_lock.h"

void Payload(uint64_t item_count, sme::AdaptiveSpinLock& spin_lock)
{
    auto tid = gettid();

    std::string begin_msg = "BEGIN:" + std::to_string(tid) + "\n";

    std::cout << begin_msg << std::flush;

    std::unique_lock ul{spin_lock};

    std::cout << "ACQUIRED:" << tid << std::endl;

    auto begin = std::numeric_limits<uint64_t>::max();
    auto end = begin - item_count;

    std::list<std::string> data;

    for (auto i = begin; i > end; i--) {
        std::string s = std::to_string(i);
        data.push_back(s);
    }

    data.sort();

    for (const auto& s : data)
        std::cout << tid << ": " << s << std::endl;

    ul.unlock();

    std::cout << "RELEASED:" << tid << std::endl;
}

TEST(AdaptiveSpinLockTest, TestNotContendedLockAndUnlock)
{
    sme::AdaptiveSpinLock spin_lock;

    std::unique_lock ul{spin_lock, std::defer_lock};

    ul.lock();
}

TEST(AdaptiveSpinLockTest, TestPairedContended)
{
    sme::AdaptiveSpinLock spin_lock;

    std::atomic<bool> ready{false};
    std::vector<std::thread> thrs;

    auto thr_func = [&ready, &spin_lock](uint64_t item_count) {
        while (!ready) {
            ;
        }

        Payload(item_count, spin_lock);
    };

    for (int i = 0; i < 20; i++)
        thrs.emplace_back(std::thread{thr_func, 30});

    ready = true;

    for (auto& thr : thrs) {
        thr.join();
    }
}
