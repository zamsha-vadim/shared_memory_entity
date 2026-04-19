#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <tuple>

#include <gtest/gtest.h>

#include "sme/mdm/string.h"
#include "sme/msg_channel.h"
#include "test/shm_test_util.h"

namespace {

constexpr auto kSomeSize = 1024UL * 64;

}  // namespace

TEST(MessageChannelTest, TestCreateChannel)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    ASSERT_NO_THROW(
        sme::MessageChannel(std::move(mem_map), sme::MessageChannel::kCreate));
}

TEST(MessageChannelTest, TestChannelForValidOpen)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};

    {
        auto mem_map1 = smf.MapMemory(sme::kAllMemoryMapRequestForShared);
        sme::MessageChannel channel1(std::move(mem_map1), sme::MessageChannel::kCreate);
    }

    auto mem_map2 = smf.MapMemory(sme::kAllMemoryMapRequestForShared);
    ASSERT_NO_THROW(sme::MessageChannel(std::move(mem_map2), sme::MessageChannel::kOpen));
}

TEST(MessageChannelTest, TestChannelForInvalidOpen)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};

    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);
    ASSERT_ANY_THROW(sme::MessageChannel(std::move(mem_map), sme::MessageChannel::kOpen));
}

TEST(MessageChannelTest, TestCreateMessage)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();

    sme::IntrusivePtr<sme::Message> msg;

    ASSERT_NO_THROW(msg = writer.CreateMessage());
    ASSERT_TRUE(msg.IsValid());
    ASSERT_EQ(msg->GetReferenceCount(), 1);
}

TEST(MessageChannelTest, TestCommitMessage)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();

    sme::IntrusivePtr<sme::Message> msg = writer.CreateMessage();
    sme::MemoryDomain& domain = msg->GetAllocationDomain();

    ASSERT_ANY_THROW((void)writer.Commit(msg));

    sme::AllocationContext alloc_ctx{domain};

    const char* kSomeString = "Some text";
    auto* s = sme::Create<sme::mdm::string>(domain, kSomeString);

    ASSERT_NO_THROW(msg->SetRootObject(s, 0));

    ASSERT_NO_THROW((void)writer.Commit(msg));
    ASSERT_FALSE(msg.IsValid());

    ASSERT_TRUE(channel.GetReader().IsEmpty());
}

TEST(MessageChannelTest, TestRollbackMessage)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();

    ASSERT_FALSE(channel.GetReader().IsEmpty());

    sme::IntrusivePtr<sme::Message> msg = writer.CreateMessage();
    msg.Release();
    ASSERT_FALSE(msg.IsValid());

    ASSERT_FALSE(channel.GetReader().IsEmpty());
}

TEST(MessageChannelTest, TestCanWriteState)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();

    sme::IntrusivePtr<sme::Message> msg1 = writer.CreateMessage();
    sme::MemoryDomain& domain1 = msg1->GetAllocationDomain();

    const char* kSomeString1 = "Some text1";
    auto* s1 = sme::Create<sme::mdm::string>(
        domain1, kSomeString1, sme::mdm::MemoryDomainAllocator<char>{domain1});
    msg1->SetRootObject(s1, 0);

    ASSERT_THROW((void)writer.CreateMessage(), std::bad_alloc);

    domain1.DisableAllocationExtensible();

    sme::IntrusivePtr<sme::Message> msg2;

    ASSERT_NO_THROW(msg2 = writer.CreateMessage());
    sme::MemoryDomain& domain2 = msg2->GetAllocationDomain();

    const char* kSomeString2 = "Some text2";
    auto* s2 = sme::Create<sme::mdm::string>(
        domain2, kSomeString2, sme::mdm::MemoryDomainAllocator<char>{domain2});
    msg2->SetRootObject(s2, 0);

    ASSERT_NO_THROW((void)writer.Commit(msg2));
    ASSERT_NO_THROW((void)writer.Commit(msg1));

    ASSERT_TRUE(channel.GetReader().IsEmpty());
}

TEST(MessageChannelTest, TestWriteSomeMessages)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();

    sme::IntrusivePtr<sme::Message> msg1 = writer.CreateMessage();
    sme::MemoryDomain& domain1 = msg1->GetAllocationDomain();

    const char* kSomeString1 = "Some text1";
    auto* s1 = sme::Create<sme::mdm::string>(
        domain1, kSomeString1, sme::mdm::MemoryDomainAllocator<char>{domain1});
    msg1->SetRootObject(s1, 0);

    domain1.DisableAllocationExtensible();

    sme::IntrusivePtr<sme::Message> msg2 = writer.CreateMessage();
    sme::MemoryDomain& domain2 = msg2->GetAllocationDomain();

    const char* kSomeString2 = "Some text2";
    auto* s2 = sme::Create<sme::mdm::string>(
        domain2, kSomeString2, sme::mdm::MemoryDomainAllocator<char>{domain2});
    msg2->SetRootObject(s2, 0);

    msg2.Release();

    sme::IntrusivePtr<sme::Message> msg3 = writer.CreateMessage();
    sme::MemoryDomain& domain3 = msg3->GetAllocationDomain();

    const char* kSomeString3 = "Some text3";
    auto* s3 = sme::Create<sme::mdm::string>(
        domain3, kSomeString3, sme::mdm::MemoryDomainAllocator<char>{domain3});
    msg3->SetRootObject(s3, 0);

    ASSERT_NO_THROW((void)writer.Commit(msg1));
    ASSERT_NO_THROW((void)writer.Commit(msg3));

    ASSERT_TRUE(channel.GetReader().IsEmpty());
}

TEST(MessageChannelTest, TestReadForEmpty)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageReader& reader = channel.GetReader();

    ASSERT_FALSE(reader.IsEmpty());
}

TEST(MessageChannelTest, TestReadMessage)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();
    sme::MessageReader& reader = channel.GetReader();

    sme::IntrusivePtr<sme::Message> out_msg = writer.CreateMessage();
    sme::MemoryDomain& out_domain = out_msg->GetAllocationDomain();

    const char* kSomeString = "Some text";
    auto* out_s = sme::Create<sme::mdm::string>(
        out_domain, kSomeString, sme::mdm::MemoryDomainAllocator<char>{out_domain});
    out_msg->SetRootObject(out_s, 0);

    (void)writer.Commit(out_msg);

    ASSERT_TRUE(reader.IsEmpty());

    sme::IntrusivePtr<sme::Message> in_msg;

    ASSERT_NO_THROW(std::tie(std::ignore, in_msg) = reader.Read());
    ASSERT_TRUE(in_msg.IsValid());
    ASSERT_EQ(in_msg->GetReferenceCount(), 1);
    ASSERT_FALSE(reader.IsEmpty());

    sme::Pointer<sme::mdm::string> root_obj = in_msg->GetRootObjectDefintion().object;
    ASSERT_TRUE(root_obj != nullptr);

    auto* in_s = std::launder<sme::mdm::string>(root_obj.GetAddress());
    ASSERT_EQ(*in_s, *out_s);
}

TEST(MessageChannelTest, TestWriteAndReadSomeMessagesForPrivateMemory)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForPrivate);

    sme::MessageChannel channel{std::move(mem_map), sme::MessageChannel::kCreate};
    sme::MessageWriter& writer = channel.GetWriter();
    sme::MessageReader& reader = channel.GetReader();

    constexpr auto kMsgCount = 100U;
    std::vector<std::string> chk_strs;
    chk_strs.reserve(kMsgCount);

    const char* kSomeString = "Some text for test";

    for (auto k = 0U; k < 100U; k++) {
        for (auto i = 0U; i < kMsgCount; i++) {
            std::string s = kSomeString + std::to_string(i + 1);
            chk_strs.push_back(s);

            sme::IntrusivePtr<sme::Message> out_msg = writer.CreateMessage();
            sme::MemoryDomain& out_domain = out_msg->GetAllocationDomain();

            auto* out_s = sme::Create<sme::mdm::string>(
                out_domain, s.c_str(), sme::mdm::MemoryDomainAllocator<char>{out_domain});
            out_msg->SetRootObject(out_s, 0);

            ASSERT_NO_THROW((void)writer.Commit(out_msg));
        }

        ASSERT_TRUE(reader.IsEmpty());

        auto i = 0U;
        while (reader.IsEmpty()) {
            sme::IntrusivePtr<sme::Message> in_msg;

            ASSERT_NO_THROW(std::tie(std::ignore, in_msg) = reader.Read());
            ASSERT_TRUE(in_msg.IsValid());

            sme::Pointer<sme::mdm::string> root_obj =
                in_msg->GetRootObjectDefintion().object;
            ASSERT_TRUE(root_obj != nullptr);

            auto* in_s = std::launder<sme::mdm::string>(root_obj.GetAddress());
            ASSERT_EQ(in_s->compare(chk_strs.at(i++)), 0);
        }

        ASSERT_EQ(i, kMsgCount);
    }
}

TEST(MessageChannelTest, TestWriteAndReadSomeMessagesForSharedMemory)
{
    auto smf = CreateTestSharedMemoryFile(GetUniqueName(), kSomeSize);
    SharedMemoryFileDeleter smf_del{smf};
    auto mem_map1 = smf.MapMemory(sme::kAllMemoryMapRequestForShared);
    auto mem_map2 = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

    sme::MessageChannel in_channel{std::move(mem_map1), sme::MessageChannel::kCreate};

    const char* kSomeString = "Some text";
    constexpr auto kMsgCount = 100U;

    std::vector<std::string> chk_strs;
    chk_strs.reserve(kMsgCount);

    for (auto i = 0U; i < kMsgCount; i++) {
        std::string s = kSomeString + std::to_string(i + 1);
        chk_strs.push_back(s);
    }

    auto pid = fork();
    if (pid == 0) {
        sme::MessageChannel out_channel{std::move(mem_map2), sme::MessageChannel::kOpen};
        sme::MessageWriter& writer = out_channel.GetWriter();

        for (const auto& s : chk_strs) {
            sme::IntrusivePtr<sme::Message> out_msg = writer.CreateMessage();
            sme::MemoryDomain& out_domain = out_msg->GetAllocationDomain();

            auto* out_s = sme::Create<sme::mdm::string>(
                out_domain, s.c_str(), sme::mdm::MemoryDomainAllocator<char>{out_domain});
            out_msg->SetRootObject(out_s, 0);

            (void)writer.Commit(out_msg);
        }

        exit(EXIT_SUCCESS);

    } else if (pid != -1) {
        int wait_stat{};

        if (waitpid(pid, &wait_stat, 0) == -1)
            FAIL() << std::strerror(errno);
        if (!WIFEXITED(wait_stat) || WEXITSTATUS(wait_stat) != EXIT_SUCCESS)
            FAIL() << "Output message process exited with an unexpected error";

        sme::MessageReader& reader = in_channel.GetReader();

        ASSERT_TRUE(reader.IsEmpty());

        auto i = 0U;
        while (reader.IsEmpty() && i < chk_strs.size()) {
            sme::IntrusivePtr<sme::Message> in_msg;

            ASSERT_NO_THROW(std::tie(std::ignore, in_msg) = reader.Read());
            ASSERT_TRUE(in_msg);

            sme::Pointer<sme::mdm::string> root_obj =
                in_msg->GetRootObjectDefintion().object;
            ASSERT_TRUE(root_obj != nullptr);

            const auto* in_s = std::launder<sme::mdm::string>(root_obj.GetAddress());
            ASSERT_EQ(in_s->compare(chk_strs[i++]), 0);
        }

        ASSERT_EQ(i, chk_strs.size());
    } else {
        FAIL() << std::strerror(errno);
    }
}
