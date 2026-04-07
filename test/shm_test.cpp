#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "sme/mapped_obj.h"
#include "sme/shm.h"
#include "test/shm_test_util.h"

namespace {

struct Foo {
    char c{};
    int i{};
    double d{};
    std::array<char, 50> s;
};

}  // namespace

TEST(SharedMemoryFileTest, TestValidConstructForEmptySize)
{
    const auto name = GetUniqueName();

    auto smf = CreateTestSharedMemoryFile(name, 0);
    SharedMemoryFileDeleter smfd{smf};

    EXPECT_TRUE(smf.IsValid());
    EXPECT_EQ(smf.GetPath(), name);
    EXPECT_EQ(smf.GetSize(), 0);

    EXPECT_ANY_THROW((void)smf.MapMemory(sme::kAllMemoryMapRequestAsShared));
}

TEST(SharedMemoryFileTest, TestValidConstructForInitialSize)
{
    const auto name = GetUniqueName();
    constexpr auto kSomeSize = 123456U;

    auto smf = CreateTestSharedMemoryFile(name, kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    EXPECT_TRUE(smf.IsValid());
    EXPECT_EQ(smf.GetPath(), name);
    EXPECT_EQ(smf.GetSize(), kSomeSize);

    EXPECT_TRUE(smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
}

TEST(SharedMemoryFileTest, TestConstructForInvalidName)
{
    constexpr std::string_view kEmptyName{};

    EXPECT_THROW(sme::SharedMemoryFile(kEmptyName, kShmOpenFlags, kShmUserRwRights),
                 std::system_error);
}

TEST(SharedMemoryFileTest, TestCopyConstructor)
{
    constexpr auto kSomeSize = 1234U;

    auto src_smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{src_smf};

    auto dest_smf{src_smf};

    EXPECT_EQ(src_smf.GetPath(), dest_smf.GetPath());
    EXPECT_EQ(src_smf.GetSize(), dest_smf.GetSize());
    EXPECT_EQ(src_smf.IsValid(), dest_smf.IsValid());

    EXPECT_TRUE(src_smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
    EXPECT_TRUE(dest_smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
}

TEST(SharedMemoryFileTest, TestMoveConstructor)
{
    const auto name = GetUniqueName();
    constexpr auto kSomeSize = 1234U;

    auto src_smf = CreateTestSharedMemoryFile(name, kSomeSize);
    SharedMemoryFileDeleter smfd{src_smf};

    auto dest_smf{std::move(src_smf)};

    EXPECT_EQ(dest_smf.GetPath(), name);
    EXPECT_ANY_THROW(src_smf.GetPath());

    EXPECT_EQ(dest_smf.GetSize(), kSomeSize);
    EXPECT_ANY_THROW(src_smf.GetSize());

    EXPECT_FALSE(src_smf.IsValid());
    EXPECT_TRUE(dest_smf.IsValid());

    EXPECT_ANY_THROW((void)src_smf.MapMemory(sme::kAllMemoryMapRequestAsShared));
    EXPECT_TRUE(dest_smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
}

TEST(SharedMemoryFileTest, TestCopyOperator)
{
    constexpr auto kSomeSize = 1234U;

    auto src_smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{src_smf};

    auto dest_smf = src_smf;

    EXPECT_EQ(src_smf.GetPath(), dest_smf.GetPath());
    EXPECT_EQ(src_smf.GetSize(), dest_smf.GetSize());
    EXPECT_EQ(src_smf.IsValid(), dest_smf.IsValid());

    EXPECT_TRUE(src_smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
    EXPECT_TRUE(dest_smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
}

TEST(SharedMemoryFileTest, TestMoveOperator)
{
    const auto name = GetUniqueName();
    constexpr auto kSomeSize = 1234U;

    auto src_smf = CreateTestSharedMemoryFile(name, kSomeSize);
    SharedMemoryFileDeleter smfd{src_smf};

    sme::SharedMemoryFile dest_smf;

    dest_smf = std::move(src_smf);

    EXPECT_EQ(dest_smf.GetPath(), name);
    EXPECT_ANY_THROW(src_smf.GetPath());

    EXPECT_EQ(dest_smf.GetSize(), kSomeSize);
    EXPECT_ANY_THROW(src_smf.GetSize());

    EXPECT_FALSE(src_smf.IsValid());
    EXPECT_TRUE(dest_smf.IsValid());

    EXPECT_ANY_THROW((void)src_smf.MapMemory(sme::kAllMemoryMapRequestAsShared));
    EXPECT_TRUE(dest_smf.MapMemory(sme::kAllMemoryMapRequestAsShared) != nullptr);
}

TEST(SharedMemoryFileTest, TestFileSize)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), 0);
    SharedMemoryFileDeleter smfd{smf};

    ASSERT_EQ(smf.GetSize(), 0);

    constexpr auto kSomeSize = 12345U;

    smf.SetSize(kSomeSize);
    EXPECT_EQ(smf.GetSize(), kSomeSize);

    EXPECT_EQ(OpenTestSharedMemoryFile(smf.GetPath()).GetSize(), kSomeSize);
}

TEST(SharedMemoryFileTest, TestFileClose)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), 0);
    SharedMemoryFileDeleter smfd{smf};

    smf.Close();

    EXPECT_FALSE(smf.IsValid());
    EXPECT_THROW(smf.GetPath(), std::runtime_error);
    EXPECT_THROW(smf.GetSize(), std::runtime_error);
}

TEST(SharedMemoryFileTest, TestFileUnlink)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), 0);
    SharedMemoryFileDeleter smfd{smf};

    EXPECT_NO_THROW(sme::SharedMemoryFile::Unlink(smf.GetPath()));
    EXPECT_THROW(sme::SharedMemoryFile::Unlink(smf.GetPath()), std::system_error);
}

TEST(SharedMemoryFileTest, TestFileUnlinkIfExists)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), 0);
    SharedMemoryFileDeleter smfd{smf};

    ASSERT_NO_THROW(sme::SharedMemoryFile::Unlink(smf.GetPath()));
    EXPECT_NO_THROW(sme::SharedMemoryFile::UnlinkIfExists(smf.GetPath()));
}

TEST(SharedMemoryTest, TestValidMemoryMap)
{
    constexpr auto kSomeSize = 12345U;

    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    sme::MemoryMap mem_map;

    ASSERT_NO_THROW(mem_map = smf.MapMemory(sme::kAllMemoryMapRequestAsShared));

    EXPECT_EXIT(
        {
            FillMappedMemory(mem_map, 'x', kSomeSize);
            exit(0);
        },
        testing::ExitedWithCode(0), "");
}

TEST(SharedMemoryTest, TestInvalidMemoryMap)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), 0);
    SharedMemoryFileDeleter smfd{smf};

    smf.Close();
    ASSERT_ANY_THROW((void)smf.MapMemory(sme::kAllMemoryMapRequestAsShared));
}

TEST(SharedMemoryTest, TestInterprocessCommutication)
{
    auto name = GetUniqueName();
    constexpr auto kSomeSize = 12345U;

    auto smf = CreateTestSharedMemoryFile(name, kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    constexpr char value = 'x';

    ASSERT_EXIT(
        {
            auto other_smf = OpenTestSharedMemoryFile(name);

            auto other_mem = other_smf.MapMemory(sme::kAllMemoryMapRequestAsShared);
            FillMappedMemory(other_mem, value, kSomeSize);

            exit(EXIT_SUCCESS);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");

    ASSERT_EXIT(
        {
            auto mem = smf.MapMemory(sme::kAllMemoryMapRequestAsShared);
            char* area = static_cast<char*>(mem.GetAddress());

            for (auto i = 0U; i < kSomeSize; i++)
                if (area[i] != value)
                    exit(EXIT_FAILURE);
            exit(EXIT_SUCCESS);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(SharedMemoryTest, TestAnonymousMemory)
{
    const char* kSomeStr1 = "No War!";
    const char* kSomeStr2 = "Hello Peace";

    auto shm = sme::SharedMemoryFile::MapAnonymousMemory(
        100, sme::MemoryMap::ShareType::kShared);

    char* s = static_cast<char*>(shm.GetAddress());
    strcpy(s, kSomeStr1);

    ASSERT_EXIT(
        {
            if (strcmp(s, kSomeStr1) != 0)
                exit(EXIT_FAILURE);

            strcpy(s, kSomeStr2);
            exit(EXIT_SUCCESS);
        },
        testing::ExitedWithCode(EXIT_SUCCESS), "");

    ASSERT_STREQ(s, kSomeStr2);
}

TEST(SharedMemoryTest, TestSharedObject)
{
    constexpr auto kSomeSize = 1024U;
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    sme::MemoryMapRequest mmr{kSomeSize, 0, sme::MemoryMap::ShareType::kShared};

    {
        auto mem0 = smf.MapMemory(mmr);

        using ObjType = std::array<char, kSomeSize>*;
        ObjType* obj0{};

        ASSERT_NO_THROW(obj0 = sme::Construct<ObjType>(mem0, 0));
        ASSERT_TRUE(obj0 != nullptr);
    }

    constexpr int kSomeChar{'V'};
    constexpr int kSomeInt{123};
    constexpr double kSomeDouble{456.789};
    const char* kSomeStr = "Hello Peace";

    {
        auto mem1 = smf.MapMemory(mmr);

        Foo* obj1{};

        ASSERT_NO_THROW(obj1 = sme::Construct<Foo>(mem1, 0));
        ASSERT_TRUE(obj1 != nullptr);

        obj1->c = kSomeChar;
        obj1->i = kSomeInt;
        obj1->d = kSomeDouble;
        strcpy(obj1->s.data(), kSomeStr);
    }

    {
        auto mem2 = smf.MapMemory(mmr);

        Foo* obj2{};

        ASSERT_NO_THROW(obj2 = &sme::GetObject<Foo>(mem2, 0));
        ASSERT_TRUE(obj2 != nullptr);

        EXPECT_EQ(obj2->c, kSomeChar);
        EXPECT_EQ(obj2->i, kSomeInt);
        EXPECT_DOUBLE_EQ(obj2->d, kSomeDouble);
        EXPECT_STREQ(obj2->s.data(), kSomeStr);
    }
}

TEST(SharedMemoryTest, TestCreateForValidLocation)
{
    constexpr auto kSomeSize = 1024U;

    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    sme::MemoryMapRequest mmr{kSomeSize, 0, sme::MemoryMap::ShareType::kShared};
    auto mem = smf.MapMemory(mmr);

    ASSERT_NO_THROW((void)sme::Construct<Foo>(mem, alignof(Foo)));
    ASSERT_NO_THROW((void)sme::Construct<Foo>(mem, alignof(Foo) * 2));
    ASSERT_NO_THROW((void)sme::Construct<Foo>(mem, kSomeSize - sizeof(Foo)));

    using BigSizeType = std::array<char, kSomeSize>;

    ASSERT_NO_THROW((void)sme::Construct<BigSizeType>(mem, 0));

    const auto kSomeLastPos = mem.GetSize() - sizeof(BigSizeType);
    ASSERT_NO_THROW((void)sme::Construct<BigSizeType>(mem, kSomeLastPos));
}

TEST(SharedMemoryTest, TestGetObjectForValidLocation)
{
    constexpr auto kSomeSize = 1024U;

    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    sme::MemoryMapRequest mmr{kSomeSize, 0, sme::MemoryMap::ShareType::kShared};
    auto mem = smf.MapMemory(mmr);

    ASSERT_NO_THROW((void)sme::GetObject<Foo>(mem, alignof(Foo)));
    ASSERT_NO_THROW((void)sme::GetObject<Foo>(mem, alignof(Foo) * 2));
    ASSERT_NO_THROW((void)sme::GetObject<Foo>(mem, kSomeSize - sizeof(Foo)));

    using BigSizeType = std::array<char, kSomeSize>;

    ASSERT_NO_THROW((void)sme::GetObject<BigSizeType>(mem, 0));
    const auto kSomeLastPos = mem.GetSize() - sizeof(BigSizeType);
    ASSERT_NO_THROW((void)sme::GetObject<BigSizeType>(mem, kSomeLastPos));
}

TEST(SharedMemoryTest, TestCreateForInvalidLocation)
{
    constexpr auto kSomeSize = 1024U;

    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    sme::MemoryMapRequest mmr{kSomeSize, 0, sme::MemoryMap::ShareType::kShared};
    auto mem = smf.MapMemory(mmr);

    ASSERT_THROW((void)sme::Construct<Foo>(mem, alignof(Foo) + 1), std::logic_error);
    ASSERT_THROW((void)sme::Construct<Foo>(mem, alignof(Foo) * 2 + 1), std::logic_error);
    ASSERT_THROW((void)sme::Construct<Foo>(mem, alignof(Foo) * 2 - 1), std::logic_error);
    ASSERT_THROW((void)sme::Construct<Foo>(mem, kSomeSize - sizeof(Foo) - 1),
                 std::logic_error);
    ASSERT_THROW((void)sme::Construct<Foo>(mem, kSomeSize - sizeof(Foo) + 1),
                 std::logic_error);

    using InvalidBigSizeType = std::array<char, kSomeSize + 1>;
    ASSERT_THROW((void)sme::Construct<InvalidBigSizeType>(mem, 0), std::logic_error);

    using ValidBigSizeType = std::array<char, kSomeSize>;
    const auto kSomeInvalidPos = mem.GetSize() - sizeof(ValidBigSizeType) + 1;
    ASSERT_THROW((void)sme::Construct<ValidBigSizeType>(mem, kSomeInvalidPos),
                 std::logic_error);
}

TEST(SharedMemoryTest, TestGetSharedObjectForInvalidLocation)
{
    constexpr auto kSomeSize = 1024U;

    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smfd{smf};

    sme::MemoryMapRequest mmr{kSomeSize, 0, sme::MemoryMap::ShareType::kShared};
    auto mem = smf.MapMemory(mmr);

    ASSERT_THROW((void)sme::GetObject<Foo>(mem, alignof(Foo) + 1), std::logic_error);
    ASSERT_THROW((void)sme::GetObject<Foo>(mem, alignof(Foo) * 2 + 1), std::logic_error);
    ASSERT_THROW((void)sme::GetObject<Foo>(mem, alignof(Foo) * 2 - 1), std::logic_error);
    ASSERT_THROW((void)sme::GetObject<Foo>(mem, kSomeSize - sizeof(Foo) - 1),
                 std::logic_error);
    ASSERT_THROW((void)sme::GetObject<Foo>(mem, kSomeSize - sizeof(Foo) + 1),
                 std::logic_error);

    using InvalidBigSizeType = std::array<char, kSomeSize + 1>;
    ASSERT_THROW((void)sme::GetObject<InvalidBigSizeType>(mem, 0), std::logic_error);

    using ValidBigSizeType = std::array<char, kSomeSize>;

    const auto kSomeNotAlignedPos = alignof(ValidBigSizeType) + 1;
    ASSERT_THROW((void)sme::GetObject<ValidBigSizeType>(mem, kSomeNotAlignedPos),
                 std::logic_error);
    const auto kSomeInvalidLastPos = mem.GetSize() - sizeof(ValidBigSizeType) + 1;
    ASSERT_THROW((void)sme::GetObject<ValidBigSizeType>(mem, kSomeInvalidLastPos),
                 std::logic_error);
}
