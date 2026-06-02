#include <algorithm>
#include <climits>
#include <deque>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "sme/adapt_spin_lock.h"

void Payload(uint64_t try_count, uint64_t item_count, sme::AdaptiveSpinLock& spin_lock)
{
    auto tid = gettid();

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
        std::string err_msg =
            "\n###ERROR:" + std::to_string(tid) + ": " + ex.what() + "\n";
        std::cout << std::flush << err_msg << std::flush;

        abort();
    }
}

TEST(AdaptiveSpinLockTest, TestNotContendedLockAndUnlock)
{
    sme::AdaptiveSpinLock spin_lock;

    std::unique_lock ul{spin_lock, std::defer_lock};

    ul.lock();
}

TEST(AdaptiveSpinLockTest, TestPairedContended)
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
    //uint64_t try_count = 10'000;
    uint64_t try_count = 100'000UL;
    //uint64_t try_count = 1'000'000UL;

    for (int i = 0; i < 4; i++)
        thrs.emplace_back(std::thread{thr_func, try_count, 10});

    ready = true;

    for (auto& thr : thrs)
        thr.join();
}
