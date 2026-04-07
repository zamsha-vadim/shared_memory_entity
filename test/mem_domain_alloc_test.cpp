#include <array>
#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

#include "sme/mdm/deque.h"
#include "sme/mdm/forward_list.h"
#include "sme/mdm/list.h"
#include "sme/mdm/map.h"
#include "sme/mdm/set.h"
#include "sme/mdm/string.h"
#include "sme/mdm/unique_ptr.h"
#include "sme/mdm/unordered_map.h"
#include "sme/mdm/unordered_set.h"
#include "sme/mdm/vector.h"

namespace {

constexpr size_t kSomeSpaceSize{1024UL * 128};
std::array<char, kSomeSpaceSize> some_memory{};

auto CreateTestMemorySpace(size_t size = some_memory.size()) -> sme::MemorySpace
{
    assert(size <= some_memory.size());
    some_memory.fill('?');
    return sme::MemorySpace{sme::Pointer<void>{some_memory.data()}, size};
}

[[maybe_unused]] void PrintAllocations(const sme::MemorySpace& mem_space)
{
    for (const auto& alloc : mem_space) {
        std::cout << '[' << alloc.block << "-"
                  << static_cast<const void*>(static_cast<const char*>(alloc.block) +
                                              alloc.size)
                  << ", pos=" << alloc.position << ", size=" << alloc.size << '('
                  << (alloc.size - sme::kMemorySpaceBlockHeaderSize) << "), "
                  << (alloc.free ? "free" : "used") << "]\n";
    }
    std::cout << std::endl;
}

const char* kSomeChars =
    "00000000001111111111222222222233333333334444444444"
    "55555555556666666666777777777788888888889999999999";

}  // namespace

TEST(MemoryDomainAllocTest, TestBasicAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    sme::mdm::MemoryDomainAllocator<char> malc{*mem_domain};

    auto some_size{123};

    auto ptr = malc.allocate(some_size);
    auto dup_ptr = ptr;

    malc.deallocate(ptr, some_size);
}

TEST(MemoryDomainAllocTest, TestBasicAllocationUntilExhausted)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    sme::mdm::MemoryDomainAllocator<char> malc{*mem_domain};
    auto some_size{123};

    EXPECT_THROW(
        {
            for (;;)
                (void)malc.allocate(some_size);
        },
        std::bad_alloc);
}

TEST(MemoryDomainAllocTest, TestStringAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);

    constexpr auto some_size{128U};

    sme::mdm::string s1(some_size, 'X', malc);
    sme::mdm::string s2(some_size, 'Y', malc);

    sme::mdm::string s3{kSomeChars, malc};
    ASSERT_NO_THROW(s3[0] = 'V');
    ASSERT_NO_THROW(s3 += "AZ");

    EXPECT_NO_THROW({
        auto other_size = some_size * 2;
        for (auto i = 0U; i < other_size; i++)
            s1.push_back('Z');
    });

    EXPECT_NO_THROW({
        auto other_size = 2000U;
        char chr{1};
        for (auto i = 0U; i < other_size; i++, chr++) {
            s2.insert(s2.begin(), chr);
            ASSERT_EQ(s2[0], chr);
            ASSERT_EQ(*s2.cbegin(), chr);
        }
    });

    EXPECT_NO_THROW({
        for (auto iter = s1.cbegin(); iter != s1.cend(); ++iter)
            (void)*iter;
    });

    EXPECT_NO_THROW(s1 += s2 + s3);
    EXPECT_NO_THROW(s1.clear());
    EXPECT_NO_THROW(s1.shrink_to_fit());
}

TEST(MemoryDomainAllocTest, TestVectorAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::vector<uint64_t>>(*mem_domain);

    constexpr auto some_size{123U};
    constexpr uint64_t def_value{1234567UL};

    sme::mdm::vector<uint64_t> values(some_size, def_value, malc);
    // std::vector<uint64_t> values(some_size, def_value);

    for (auto i = 0U; i < values.size(); i++) {
        ASSERT_EQ(values.at(i), def_value);
        ASSERT_NO_THROW(values.at(i) = i);
        ASSERT_EQ(values.at(i), i);
    }

    ASSERT_NO_THROW(values.resize(0));
    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.shrink_to_fit());

    constexpr auto another_size{some_size + 2000U};

    for (auto i = 0U; i < another_size; i++) {
        ASSERT_NO_THROW(values.insert(values.cbegin(), i));
        ASSERT_EQ(values[0], i);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--)
        ASSERT_EQ(*iter, chk_value);

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestCharVectorAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::vector<char>>(*mem_domain);

    constexpr auto some_size{123U};
    constexpr char def_value{'V'};

    sme::mdm::vector<char> values(some_size, def_value, malc);
    // std::vector<char> values(some_size, def_value);

    for (auto i = 0U; i < values.size(); i++) {
        ASSERT_EQ(values.at(i), def_value);

        auto v = static_cast<char>(i + 1);
        ASSERT_NO_THROW(values.at(i) = v);
        ASSERT_EQ(values.at(i), v);
    }

    ASSERT_NO_THROW(values.resize(0));
    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.shrink_to_fit());

    constexpr auto another_size{some_size + 2000U};

    for (auto i = 0U; i < another_size; i++) {
        auto v = static_cast<char>(i);

        ASSERT_NO_THROW(values.insert(values.cbegin(), v));
        ASSERT_EQ(values[0], v);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        auto v = static_cast<char>(chk_value);
        ASSERT_EQ(*iter, v);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestStringVectorAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::vector<sme::mdm::string>>(
        *mem_domain);
    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);

    const sme::mdm::string kSomeString{kSomeChars, str_alc};
    constexpr auto some_size{20U};

    sme::mdm::vector<sme::mdm::string> values(some_size, kSomeString, malc);

    for (auto i = 0U; i < values.size(); i++) {
        auto chk_s = values.at(i);
        chk_s += std::to_string(i);

        ASSERT_NO_THROW(values.at(i) += std::to_string(i));
        ASSERT_EQ(values.at(i), chk_s);
    }

    // ASSERT_NO_THROW(values.resize(1)); // NOTE: Not compiled
    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.shrink_to_fit());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        auto s = kSomeString;
        s += std::to_string(i);

        ASSERT_NO_THROW(values.insert(values.cbegin(), s));

        ASSERT_EQ(*values.cbegin(), s);
        ASSERT_EQ(values[0], s);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        sme::mdm::string s = kSomeString;
        s += std::to_string(chk_value);

        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestCharDequeAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::deque<char>>(*mem_domain);

    constexpr auto some_size{12U};
    constexpr char def_value{'V'};

    sme::mdm::deque<char> values(some_size, def_value, malc);
    // std::deque<char> values;

    for (auto i = 0U; i < values.size(); i++) {
        ASSERT_EQ(values.at(i), def_value);

        char ch = static_cast<char>(i + 1);
        ASSERT_NO_THROW(values.at(i) = ch);
        ASSERT_EQ(values.at(i), ch);
    }

    for (auto i = 0U; i < values.size(); i++)
        ASSERT_EQ(values[i], static_cast<char>(i + 1));

    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.resize(0));
    ASSERT_NO_THROW(values.shrink_to_fit());

    auto another_size{some_size + 20U};

    for (auto i = 0U; i < another_size; i++) {
        auto v = static_cast<char>(i + 1);

        ASSERT_NO_THROW(values.insert(values.cbegin(), v));
        ASSERT_EQ(values[0], v);

        if (i > 0) {
            ASSERT_EQ(values[1], static_cast<char>(i));
        }
    }

    auto last_chk_value{another_size};

    auto chk_value = last_chk_value;
    for (auto i = 0U; i < values.size(); i++, chk_value--) {
        ASSERT_EQ(values[i], static_cast<char>(chk_value));
    }

    chk_value = last_chk_value;
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--)
        ASSERT_EQ(*iter, chk_value);

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestDequeAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::deque<uint64_t>>(*mem_domain);

    constexpr auto some_size{123U};
    sme::mdm::deque<uint64_t> values(some_size, 1234567LU, malc);

    for (auto i = 0U; i < values.size(); i++) {
        auto some_value = i + 111111;
        ASSERT_NO_THROW(values.at(i) = some_value);
    }

    for (auto i = 0U; i < values.size(); i++) {
        auto chk_value = i + 111111;
        ASSERT_EQ(values[i], chk_value);
    }

    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.resize(0));
    ASSERT_NO_THROW(values.shrink_to_fit());

    auto another_size{some_size + 20U};

    for (auto i = 0U; i < another_size; i++) {
        auto some_value = i + 1;
        ASSERT_NO_THROW(values.insert(values.cbegin(), some_value));
        ASSERT_EQ(values[0], some_value);
    }

    auto last_chk_value{another_size};

    auto chk_value = last_chk_value;
    for (auto i = 0U; i < values.size(); i++, chk_value--) {
        ASSERT_EQ(values[i], chk_value);
    }

    chk_value = last_chk_value;
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--)
        ASSERT_EQ(*iter, chk_value);

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestStringDequeAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::deque<sme::mdm::string>>(*mem_domain);

    constexpr auto some_size{123U};
    sme::mdm::deque<sme::mdm::string> values(some_size, sme::mdm::string{str_alc}, malc);

    for (auto i = 0U; i < values.size(); i++)
        ASSERT_NO_THROW(values.at(i) = std::to_string(i));

    // ASSERT_NO_THROW(values.resize(0)); // NOTE: not compiled
    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.shrink_to_fit());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        const sme::mdm::string s{std::to_string(i), str_alc};
        ASSERT_NO_THROW(values.insert(values.cbegin(), s));
        ASSERT_EQ(values.at(0), s);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        const sme::mdm::string s{std::to_string(chk_value), str_alc};
        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::list<uint64_t>>(*mem_domain);

    auto some_size{1U};
    constexpr uint64_t def_value{11223344556677UL};

    sme::mdm::list<uint64_t> values{some_size, def_value, malc};
    // std::list<uint64_t> values{some_size, def_value};

    for (auto value : values) {
        ASSERT_EQ(value, def_value);
    }

    ASSERT_NO_THROW(values.clear());

    auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        ASSERT_NO_THROW(values.insert(values.cbegin(), i));
        ASSERT_EQ(values.front(), i);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        ASSERT_EQ(*iter, chk_value);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestStringListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::list<sme::mdm::string>>(*mem_domain);

    constexpr auto some_size{123U};
    const sme::mdm::string def_value{kSomeChars, str_alc};

    sme::mdm::list<sme::mdm::string> values(some_size, def_value, malc);

    values.push_back(def_value);

    for (auto value : values)
        ASSERT_EQ(value, def_value);

    ASSERT_NO_THROW(values.clear());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        const sme::mdm::string s{std::to_string(i), str_alc};
        ASSERT_NO_THROW(values.insert(values.begin(), s));
        ASSERT_EQ(values.front(), s);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        const sme::mdm::string s{std::to_string(chk_value), str_alc};
        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestForwardListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::forward_list<uint64_t>>(*mem_domain);

    constexpr auto some_size{123U};
    constexpr uint64_t def_value{1234567UL};

    sme::mdm::forward_list<uint64_t> values{some_size, def_value, malc};
    // std::forward_list<uint64_t> values(some_size, def_value);

    values.insert_after(values.cbegin(), def_value);

    for (auto value : values)
        EXPECT_EQ(value, def_value);

    ASSERT_NO_THROW(values.clear());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++)
        ASSERT_NO_THROW(values.push_front(i));

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--)
        ASSERT_EQ(*iter, chk_value);

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestStringForwardListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::forward_list<sme::mdm::string>>(
            *mem_domain);

    constexpr auto some_size{123U};
    const sme::mdm::string def_value{kSomeChars, str_alc};

    sme::mdm::forward_list<sme::mdm::string> values(some_size, def_value, malc);

    for (auto value : values)
        ASSERT_EQ(value, def_value);

    ASSERT_NO_THROW(values.clear());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        const sme::mdm::string s{std::to_string(i), str_alc};
        ASSERT_NO_THROW(values.push_front(s));
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        const sme::mdm::string s{std::to_string(chk_value), str_alc};
        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::map<int, sme::mdm::string>>(
        *mem_domain);

    sme::mdm::map<int, sme::mdm::string> values(
        {{1, sme::mdm::string("1", str_alc)}, {2, sme::mdm::string("2", str_alc)}}, malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::mdm::string s1(std::to_string(i), str_alc);
        auto value1 = std::make_pair(i, s1);
        ASSERT_TRUE(values.insert(value1).second);

        ASSERT_TRUE(values
                        .emplace(std::make_pair(
                            i + 1, sme::mdm::string(std::to_string(i + 1), str_alc)))
                        .second);
    }

    for (auto i = 1U; i < some_size; i++) {
        const sme::mdm::string value(std::to_string(i), str_alc);

        auto found_iter = values.find(i);
        ASSERT_TRUE(found_iter != values.cend());
        ASSERT_EQ(found_iter->second, value);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(0, sme::mdm::string(std::to_string(0), str_alc))));
}

TEST(MemoryDomainAllocTest, TestStringMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc = sme::mdm::CreateValueAllocatorFor<
        sme::mdm::map<sme::mdm::string, sme::mdm::string>>(*mem_domain);

    sme::mdm::map<sme::mdm::string, sme::mdm::string> values(
        {{sme::mdm::string("1", str_alc), sme::mdm::string("10", str_alc)},
         {sme::mdm::string("2", str_alc), sme::mdm::string("20", str_alc)}},
        malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::mdm::string key1(std::to_string(i), str_alc);
        sme::mdm::string value1(std::to_string(i * 10), str_alc);

        auto item1 = std::make_pair(key1, value1);
        ASSERT_TRUE(values.insert(item1).second);

        sme::mdm::string key2(std::to_string(i + 1), str_alc);
        sme::mdm::string value2(std::to_string((i + 1) * 10), str_alc);

        ASSERT_TRUE(values.emplace(std::make_pair(key2, value2)).second);
    }

    for (auto i = 1U; i < some_size; i++) {
        const sme::mdm::string key(std::to_string(i), str_alc);
        const sme::mdm::string value(std::to_string(i * 10), str_alc);

        auto found_iter = values.find(key);
        ASSERT_TRUE(found_iter != values.cend());
        ASSERT_EQ(found_iter->second, value);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(sme::mdm::string{std::to_string(0), str_alc},
                                      sme::mdm::string{std::to_string(0), str_alc})));
}

TEST(MemoryDomainAllocTest, TestUnorderedMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::unordered_map<int, sme::mdm::string>>(
            *mem_domain);

    sme::mdm::unordered_map<int, sme::mdm::string> values{malc};
    values = {{1, sme::mdm::string("1", str_alc)}, {2, sme::mdm::string("2", str_alc)}};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::mdm::string s1(std::to_string(i), str_alc);
        auto value1 = std::make_pair(i, s1);
        ASSERT_TRUE(values.insert(value1).second);

        ASSERT_TRUE(values
                        .emplace(std::make_pair(
                            i + 1, sme::mdm::string(std::to_string(i + 1), str_alc)))
                        .second);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(0, sme::mdm::string(std::to_string(0), str_alc))));
}

TEST(MemoryDomainAllocTest, TestStringUnorderedMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc = sme::mdm::CreateValueAllocatorFor<
        sme::mdm::unordered_map<sme::mdm::string, sme::mdm::string>>(*mem_domain);

    sme::mdm::unordered_map<sme::mdm::string, sme::mdm::string> values{malc};

    values = {{sme::mdm::string("1", str_alc), sme::mdm::string("10", str_alc)},
              {sme::mdm::string("2", str_alc), sme::mdm::string("20", str_alc)}};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::mdm::string key1(kSomeChars + std::to_string(i), str_alc);
        sme::mdm::string value1(kSomeChars + std::to_string(i * 10), str_alc);

        auto item1 = std::make_pair(key1, value1);
        ASSERT_TRUE(values.insert(item1).second);

        sme::mdm::string key2(kSomeChars + std::to_string(i + 1), str_alc);
        sme::mdm::string value2(kSomeChars + std::to_string((i + 1) * 10), str_alc);

        ASSERT_TRUE(values.emplace(std::make_pair(key2, value2)).second);
    }

    for (auto i = 3U; i < some_size; i++) {
        const sme::mdm::string key(kSomeChars + std::to_string(i), str_alc);
        const sme::mdm::string value(kSomeChars + std::to_string(i * 10), str_alc);

        auto found_iter = values.find(key);
        ASSERT_TRUE(found_iter != values.cend());
        ASSERT_EQ(found_iter->second, value);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(sme::mdm::string{std::to_string(0), str_alc},
                                      sme::mdm::string{std::to_string(0), str_alc})));
}

TEST(MemoryDomainAllocTest, TestSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc = sme::mdm::CreateValueAllocatorFor<sme::mdm::set<uint64_t>>(*mem_domain);

    sme::mdm::set<uint64_t> values{{1U, 2U}, malc};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i++)
        ASSERT_TRUE(values.emplace(i).second);

    for (auto i = 1U; i < some_size; i++)
        ASSERT_TRUE(values.find(i) != values.cend());

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestStringSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::set<sme::mdm::string>>(*mem_domain);

    sme::mdm::set<sme::mdm::string> values(
        {{sme::mdm::string("1", str_alc)}, {sme::mdm::string("2", str_alc)}}, malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i++)
        ASSERT_TRUE(values.emplace(sme::mdm::string{std::to_string(i), str_alc}).second);

    for (auto i = 1U; i < some_size; i++)
        ASSERT_TRUE(values.find(sme::mdm::string{std::to_string(i), str_alc}) !=
                    values.cend());

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestUnorderedSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::unordered_set<uint64_t>>(*mem_domain);

    sme::mdm::unordered_set<uint64_t> values{malc};
    values = {1, 2};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i++)
        ASSERT_TRUE(values.emplace(i).second);

    for (auto i = 1U; i < some_size; i++)
        ASSERT_TRUE(values.find(i) != values.cend());

    ASSERT_NO_THROW(values.clear());
}

TEST(MemoryDomainAllocTest, TestStringUnorderedSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto str_alc = sme::mdm::CreateValueAllocatorFor<sme::mdm::string>(*mem_domain);
    auto malc =
        sme::mdm::CreateValueAllocatorFor<sme::mdm::unordered_set<sme::mdm::string>>(
            *mem_domain);

    sme::mdm::unordered_set<sme::mdm::string> values{malc};
    values = {sme::mdm::string("1", str_alc), sme::mdm::string("2", str_alc)};

    ASSERT_NO_THROW(values.clear());

    constexpr auto some_size{123U};

    for (auto i = 0U; i < some_size; i++) {
        sme::mdm::string s{i, 'X', str_alc};
        ASSERT_TRUE(values.emplace(s).second);
    }

    for (auto i = 0U; i < some_size; i++) {
        sme::mdm::string s{i, 'X', str_alc};
        ASSERT_TRUE(values.find(s) != values.cend());
    }
}

TEST(MemoryDomainAllocTest, TestUniquePtrForIntegralType)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    auto obj = sme::make_unique<uint64_t>(*mem_domain);
    ASSERT_TRUE(obj != nullptr);

    ASSERT_NO_THROW(obj.reset(););
}

TEST(MemoryDomainAllocTest, TestUniquePtrForString)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    using String = sme::mdm::string;

    auto obj = sme::make_unique<String>(*mem_domain, String::allocator_type{*mem_domain});
    ASSERT_TRUE(obj != nullptr);

    ASSERT_NO_THROW(obj.reset(););
}

TEST(MemoryDomainAllocTest, TestAllocationContextConstructor)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    ASSERT_NO_THROW(sme::AllocationContext<sme::MemoryDomain>{*mem_domain});
    ASSERT_NO_THROW(sme::AllocationContext<sme::MemoryDomain>{*mem_domain});

    sme::AllocationContext<sme::MemoryDomain> ctx{*mem_domain};

    ASSERT_TRUE(ctx.GetArea() == mem_domain);
}

TEST(MemoryDomainAllocTest, TestAcquireAllocationContext)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> ctx1{*mem_domain};

    ASSERT_ANY_THROW(sme::AllocationContext<sme::MemoryDomain>{*mem_domain});

    ASSERT_NO_THROW(ctx1.Acquire());

    ctx1.Release();

    ASSERT_NO_THROW(ctx1.Acquire());
    ctx1.Release();

    ASSERT_NO_THROW(sme::AllocationContext<sme::MemoryDomain>{*mem_domain});
}

TEST(MemoryDomainAllocTest, TestReleaseAllocationContext)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> ctx{*mem_domain};

    ctx.Release();

    ASSERT_FALSE(ctx.IsAcquired());
    ASSERT_TRUE(sme::AllocationContext<sme::MemoryDomain>::CanAcquire());
    ASSERT_TRUE(ctx.GetArea() == nullptr);
}

TEST(MemoryDomainAllocTest, TestChangeAllocationContext)
{
    auto mem_space = CreateTestMemorySpace();

    auto mem_domain1 = sme::CreateMemoryDomain(mem_space);
    mem_domain1->DisableAllocationExtensible();

    auto mem_domain2 = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> ctx1{*mem_domain1};

    ASSERT_NO_THROW(ctx1.Acquire(*mem_domain2));
    ASSERT_TRUE(ctx1.GetArea() == mem_domain2);

    ctx1.Release();

    ASSERT_NO_THROW(ctx1.Acquire());
    ASSERT_TRUE(ctx1.GetArea() == mem_domain2);

    ctx1.Release();

    ASSERT_NO_THROW(sme::AllocationContext<sme::MemoryDomain>{*mem_domain1});
    ASSERT_NO_THROW(sme::AllocationContext<sme::MemoryDomain>{*mem_domain2});

    sme::AllocationContext<sme::MemoryDomain> ctx2{*mem_domain2};

    ASSERT_NO_THROW(ctx2.Acquire(*mem_domain1));
    ASSERT_TRUE(ctx2.GetArea() == mem_domain1);
}

TEST(MemoryDomainAllocTest, TestAllocationContextState)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain1 = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> ctx1{*mem_domain1};

    ASSERT_TRUE(ctx1.IsAcquired());
    ASSERT_FALSE(sme::AllocationContext<sme::MemoryDomain>::CanAcquire());

    ctx1.Release();

    ASSERT_FALSE(ctx1.IsAcquired());
    ASSERT_TRUE(sme::AllocationContext<sme::MemoryDomain>::CanAcquire());

    mem_domain1->DisableAllocationExtensible();
    auto mem_domain2 = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> ctx2{*mem_domain2};

    ASSERT_FALSE(ctx1.IsAcquired());
    ASSERT_TRUE(ctx2.IsAcquired());
    ASSERT_FALSE(sme::AllocationContext<sme::MemoryDomain>::CanAcquire());
}

TEST(MemoryDomainAllocTest, TestForNoneAllocationContext)
{
    auto mem_space = CreateTestMemorySpace();

    ASSERT_ANY_THROW(sme::mdm::string{kSomeChars});
}

TEST(MemoryDomainAllocTest, TestAllocationContextForString)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> md_ctx{*mem_domain};

    ASSERT_NO_THROW(sme::mdm::string{kSomeChars});

    const char* s_data{};

    {
        sme::mdm::string s{kSomeChars};

        s_data = s.data();

        auto addr_state = mem_domain->GetAddressState(s_data);
        ASSERT_EQ(addr_state, sme::MemoryDomain::AddressState::kUsed);
    }

    auto addr_state = mem_domain->GetAddressState(s_data);
    ASSERT_EQ(addr_state, sme::MemoryDomain::AddressState::kFree);
}

TEST(MemoryDomainAllocTest, TestAllocationContextForList)
{
    auto mem_space = CreateTestMemorySpace();
    auto mem_domain = sme::CreateMemoryDomain(mem_space);

    sme::AllocationContext<sme::MemoryDomain> md_ctx{*mem_domain};

    std::list<std::pair<const sme::mdm::string*, const char*>> chk_list;

    {
        sme::mdm::list<sme::mdm::string> lst;

        for (auto i = 0; i < 10; i++) {
            std::string s = kSomeChars + std::to_string(i);

            auto& item = lst.emplace_back(s.c_str());
            chk_list.push_back(std::make_pair(&item, item.data()));
        }

        for (auto& chk : chk_list) {
            auto addr_state = mem_domain->GetAddressState(chk.first);
            ASSERT_EQ(addr_state, sme::MemoryDomain::AddressState::kUsed);

            addr_state = mem_domain->GetAddressState(chk.second);
            ASSERT_EQ(addr_state, sme::MemoryDomain::AddressState::kUsed);
        }
    }

    for (auto& chk : chk_list) {
        auto addr_state = mem_domain->GetAddressState(chk.first);
        ASSERT_EQ(addr_state, sme::MemoryDomain::AddressState::kFree);

        addr_state = mem_domain->GetAddressState(chk.second);
        ASSERT_EQ(addr_state, sme::MemoryDomain::AddressState::kFree);
    }
}
