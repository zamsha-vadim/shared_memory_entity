#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include "sme/cond_var.h"
#include "sme/mapped_obj.h"
#include "sme/shm.h"

TEST(ConditionVariableTest, TestConstructorForDefaultPrivateType)
{
    ASSERT_NO_THROW(sme::ConditionVariable{});
    ASSERT_NO_THROW(sme::Mutex{sme::InterprocessVisibility::kPrivate});
}

TEST(ConditionVariableTest, TestConstructorForSharedType)
{
    ASSERT_NO_THROW(sme::ConditionVariable{sme::InterprocessVisibility::kShared});
}

TEST(ConditionVariableTest, TestNotifyOne) {
    sme::ConditionVariable cv;

    ASSERT_NO_THROW(cv.NotifyOne());
}

TEST(ConditionVariableTest, TestNotifyAll) {
    sme::ConditionVariable cv;

    ASSERT_NO_THROW(cv.NotifyAll());
}

TEST(ConditionVariableTest, TestWaitForNoTimeout) {
    sme::Mutex mutex;
    sme::ConditionVariable cv;

    std::unique_lock ml{mutex};

    bool res{};

    ASSERT_NO_THROW(res = cv.WaitFor(ml, std::chrono::milliseconds::zero()));
    ASSERT_FALSE(res);
}

TEST(ConditionVariableTest, TestWaitForSomeTimeout) {
    sme::Mutex mutex;
    sme::ConditionVariable cv;

    std::unique_lock ml{mutex};

    std::chrono::milliseconds timeout(500);

    bool res{};

    auto begin_time = std::chrono::steady_clock::now();

    ASSERT_NO_THROW(res = cv.WaitFor(ml, timeout));
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);

    ASSERT_FALSE(res);

    ASSERT_LE(timeout.count(), elapsed_time.count());
    ASSERT_LE(elapsed_time.count(), (timeout.count() + 50));
}

TEST(ConditionVariableTest, TestForThreadSynchronization)
{
    sme::Mutex mutex;
    sme::ConditionVariable cv;

    std::atomic<bool> wait_res{false};
    std::atomic<bool> done{false};

    const std::chrono::milliseconds timeout{300};
    const auto try_count{100U};
    const std::chrono::milliseconds cycle_timeout{timeout.count() / try_count};

    std::thread thr([&]() {
        std::unique_lock<sme::Mutex> ml{mutex};

        wait_res = cv.WaitFor(ml, timeout);
        done = true;
    });

    for (auto i = 0U; i < try_count; i++) {
        std::unique_lock<sme::Mutex> ml{mutex};
        cv.NotifyOne();
        ml.unlock();

        if (done)
            break;
        std::this_thread::sleep_for(cycle_timeout);
    }

    thr.join();
    
    ASSERT_TRUE(wait_res);
}
