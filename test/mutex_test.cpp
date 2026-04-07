#include <gtest/gtest.h>

#include "sme/mapped_obj.h"
#include "sme/mutex.h"
#include "sme/shm.h"

TEST(MutexTest, TestConstructorForDefaultPrivateType)
{
    ASSERT_NO_THROW(sme::Mutex{});
    ASSERT_NO_THROW(sme::Mutex{sme::Mutex::Type::kPrivate});
}

TEST(MutexTest, TestPrivateLock)
{
    sme::Mutex mutex{sme::Mutex::Type::kPrivate};

    ASSERT_NO_THROW(mutex.lock());
}

TEST(MutexTest, TestTryPrivateLock)
{
    sme::Mutex mutex{sme::Mutex::Type::kPrivate};

    bool locked{};

    ASSERT_NO_THROW(locked = mutex.try_lock());
    ASSERT_TRUE(locked);

    ASSERT_NO_THROW(locked = mutex.try_lock());
    ASSERT_FALSE(locked);

    mutex.unlock();

    ASSERT_NO_THROW(locked = mutex.try_lock());
    ASSERT_TRUE(locked);
}

TEST(MutexTest, TestPrivateUnlock)
{
    sme::Mutex mutex{sme::Mutex::Type::kPrivate};

    mutex.lock();

    ASSERT_NO_THROW(mutex.unlock());
}

TEST(MutexTest, TestConstructorForSharedType)
{
    ASSERT_NO_THROW(sme::Mutex{sme::Mutex::Type::kShared});
}

TEST(MutexDeathTest, TestSharedLock)
{
    auto shm = sme::SharedMemoryFile::MapAnonymousMemory(
        sizeof(sme::Mutex), sme::MemoryMap::ShareType::kShared);
    auto* mutex = sme::Construct<sme::Mutex>(shm, 0, sme::Mutex::Type::kShared);

    ASSERT_NO_THROW(mutex->lock());

    ASSERT_EXIT(
        {
            if (!mutex->try_lock())
                exit(EXIT_SUCCESS);
            else
                exit(EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");

    ASSERT_NO_THROW(mutex->unlock());

    ASSERT_EXIT(
        {
            if (mutex->try_lock())
                _exit(EXIT_SUCCESS);
            _exit(EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");

    ASSERT_TRUE(mutex->try_lock());

    mutex->~Mutex();
}

TEST(MutexDeathTest, TestSharedMutexConsistent)
{
    auto shm = sme::SharedMemoryFile::MapAnonymousMemory(
        sizeof(sme::Mutex), sme::MemoryMap::ShareType::kShared);
    auto* mutex = sme::Construct<sme::Mutex>(shm, 0, sme::Mutex::Type::kShared);

    EXPECT_EXIT(
        {
            if (mutex->try_lock())
                exit(EXIT_SUCCESS);
            else
                exit(EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");

    ASSERT_TRUE(mutex->try_lock());
    ASSERT_NO_THROW(mutex->unlock());
}

TEST(MutexDeathTest, TestSharedMutexInvalidUnlock)
{
    auto shm = sme::SharedMemoryFile::MapAnonymousMemory(
        sizeof(sme::Mutex), sme::MemoryMap::ShareType::kShared);
    auto* mutex = sme::Construct<sme::Mutex>(shm, 0, sme::Mutex::Type::kShared);

    EXPECT_EXIT(
        {
            if (mutex->try_lock())
                exit(EXIT_SUCCESS);
            else
                exit(EXIT_FAILURE);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");

    ASSERT_THROW(mutex->unlock(), std::system_error);
}
