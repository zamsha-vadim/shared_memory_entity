#include <array>
#include <stdexcept>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "sme/msp/deque.h"
#include "sme/msp/forward_list.h"
#include "sme/msp/list.h"
#include "sme/msp/map.h"
#include "sme/msp/set.h"
#include "sme/msp/string.h"
#include "sme/msp/unordered_map.h"
#include "sme/msp/unordered_set.h"
#include "sme/msp/vector.h"

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

TEST(MemorySpaceAllocTest, TestBasicAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::MemorySpaceAllocator<char> malc{*ms_ptr};
    auto some_size{123};

    auto ptr = malc.allocate(some_size);
    auto dup_ptr = ptr;

    ASSERT_TRUE(ptr != nullptr);
    auto alloc_info = mem_space.GetAllocationInfo(ptr);
    ASSERT_TRUE(alloc_info);
    ASSERT_FALSE(alloc_info->free);
    ASSERT_EQ(sme::MemorySpaceBlock::CalculateBlockSize(some_size), alloc_info->size);

    malc.deallocate(ptr, some_size);

    alloc_info = mem_space.GetAllocationInfo(ptr);
    ASSERT_TRUE(alloc_info);
    ASSERT_TRUE(alloc_info->free);
}

TEST(MemorySpaceAllocTest, TestBasicAllocationUntilExhausted)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::MemorySpaceAllocator<char> malc{*ms_ptr};
    auto some_size{123};

    EXPECT_THROW(
        {
            for (;;)
                (void)malc.allocate(some_size);
        },
        std::bad_alloc);
}

TEST(MemorySpaceAllocTest, TestStringAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type malc{*ms_ptr};
    constexpr auto some_size{128U};

    sme::msp::string s1(some_size, 'X', malc);
    sme::msp::string s2(some_size, 'Y', malc);

    sme::msp::string s3{kSomeChars, malc};
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

TEST(MemorySpaceAllocTest, TestVectorAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::vector<uint64_t>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{123U};
    constexpr uint64_t def_value{1234567UL};

    sme::msp::vector<uint64_t> values(some_size, def_value, malc);
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

TEST(MemorySpaceAllocTest, TestCharVectorAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::vector<char>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{123U};
    constexpr char def_value{'V'};

    sme::msp::vector<char> values(some_size, def_value, malc);
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

TEST(MemorySpaceAllocTest, TestStringVectorAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::vector<sme::msp::string>::allocator_type malc{*ms_ptr};

    const sme::msp::string kSomeString{kSomeChars, str_alc};
    constexpr auto some_size{20U};

    sme::msp::vector<sme::msp::string> values(some_size, kSomeString, malc);

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
        sme::msp::string s = kSomeString;
        s += std::to_string(chk_value);

        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestCharDequeAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::deque<char>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{12U};
    constexpr char def_value{'V'};

    sme::msp::deque<char> values(some_size, def_value, malc);
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

TEST(MemorySpaceAllocTest, TestDequeAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::deque<uint64_t>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{123U};

    sme::msp::deque<uint64_t> values(some_size, 1234567LU, malc);

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

TEST(MemorySpaceAllocTest, TestStringDequeAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::deque<sme::msp::string>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{123U};

    sme::msp::deque<sme::msp::string> values(some_size, sme::msp::string{str_alc}, malc);

    for (auto i = 0U; i < values.size(); i++)
        ASSERT_NO_THROW(values.at(i) = std::to_string(i));

    // ASSERT_NO_THROW(values.resize(0)); // NOTE: not compiled
    ASSERT_NO_THROW(values.clear());
    ASSERT_NO_THROW(values.shrink_to_fit());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        const sme::msp::string s{std::to_string(i), str_alc};
        ASSERT_NO_THROW(values.insert(values.cbegin(), s));
        ASSERT_EQ(values.at(0), s);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        const sme::msp::string s{std::to_string(chk_value), str_alc};
        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::list<uint64_t>::allocator_type malc{*ms_ptr};
    auto some_size{12U};
    constexpr uint64_t def_value{11223344556677UL};

    sme::msp::list<uint64_t> values(some_size, def_value, malc);
    //std::list<uint64_t> values(some_size, def_value);

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

TEST(MemorySpaceAllocTest, TestStringListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::list<sme::msp::string>::allocator_type malc{*ms_ptr};

    constexpr auto some_size{123U};
    const sme::msp::string def_value{kSomeChars, str_alc};

    sme::msp::list<sme::msp::string> values(some_size, def_value, malc);

    values.push_back(def_value);

    for (auto value : values)
        ASSERT_EQ(value, def_value);

    ASSERT_NO_THROW(values.clear());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        const sme::msp::string s{std::to_string(i), str_alc};
        ASSERT_NO_THROW(values.insert(values.begin(), s));
        ASSERT_EQ(values.front(), s);
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        const sme::msp::string s{std::to_string(chk_value), str_alc};
        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestForwardListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::forward_list<uint64_t>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{123U};
    constexpr uint64_t def_value{1234567UL};

    sme::msp::forward_list<uint64_t> values{some_size, def_value, malc};
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

TEST(MemorySpaceAllocTest, TestStringForwardListAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::forward_list<sme::msp::string>::allocator_type malc{*ms_ptr};
    constexpr auto some_size{123U};
    const sme::msp::string def_value{kSomeChars, str_alc};

    sme::msp::forward_list<sme::msp::string> values(some_size, def_value, malc);

    for (auto value : values)
        ASSERT_EQ(value, def_value);

    ASSERT_NO_THROW(values.clear());

    constexpr auto another_size{200U + some_size};

    for (auto i = 0U; i < another_size; i++) {
        const sme::msp::string s{std::to_string(i), str_alc};
        ASSERT_NO_THROW(values.push_front(s));
    }

    auto chk_value{another_size - 1};
    for (auto iter = values.cbegin(); iter != values.cend(); ++iter, chk_value--) {
        const sme::msp::string s{std::to_string(chk_value), str_alc};
        ASSERT_EQ(*iter, s);
    }

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::map<int, sme::msp::string>::allocator_type malc{*ms_ptr};

    sme::msp::map<int, sme::msp::string> values(
        {{1, sme::msp::string("1", str_alc)}, {2, sme::msp::string("2", str_alc)}}, malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::msp::string s1(std::to_string(i), str_alc);
        auto value1 = std::make_pair(i, s1);
        ASSERT_TRUE(values.insert(value1).second);

        ASSERT_TRUE(values
                        .emplace(std::make_pair(
                            i + 1, sme::msp::string(std::to_string(i + 1), str_alc)))
                        .second);
    }

    for (auto i = 1U; i < some_size; i++) {
        const sme::msp::string value(std::to_string(i), str_alc);

        auto found_iter = values.find(i);
        ASSERT_TRUE(found_iter != values.cend());
        ASSERT_EQ(found_iter->second, value);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(0, sme::msp::string(std::to_string(0), str_alc))));
}

TEST(MemorySpaceAllocTest, TestStringMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::map<sme::msp::string, sme::msp::string>::allocator_type malc{*ms_ptr};

    sme::msp::map<sme::msp::string, sme::msp::string> values(
        {{sme::msp::string("1", str_alc), sme::msp::string("10", str_alc)},
         {sme::msp::string("2", str_alc), sme::msp::string("20", str_alc)}},
        malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::msp::string key1(std::to_string(i), str_alc);
        sme::msp::string value1(std::to_string(i * 10), str_alc);

        auto item1 = std::make_pair(key1, value1);
        ASSERT_TRUE(values.insert(item1).second);

        sme::msp::string key2(std::to_string(i + 1), str_alc);
        sme::msp::string value2(std::to_string((i + 1) * 10), str_alc);

        ASSERT_TRUE(values.emplace(std::make_pair(key2, value2)).second);
    }

    for (auto i = 1U; i < some_size; i++) {
        const sme::msp::string key(std::to_string(i), str_alc);
        const sme::msp::string value(std::to_string(i * 10), str_alc);

        auto found_iter = values.find(key);
        ASSERT_TRUE(found_iter != values.cend());
        ASSERT_EQ(found_iter->second, value);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(sme::msp::string{std::to_string(0), str_alc},
                                      sme::msp::string{std::to_string(0), str_alc})));
}

TEST(MemorySpaceAllocTest, TestUnorderedMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::unordered_map<int, sme::msp::string>::allocator_type malc{*ms_ptr};

    sme::msp::unordered_map<int, sme::msp::string> values{malc};
    values = {{1, sme::msp::string("1", str_alc)}, {2, sme::msp::string("2", str_alc)}};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::msp::string s1(std::to_string(i), str_alc);
        auto value1 = std::make_pair(i, s1);
        ASSERT_TRUE(values.insert(value1).second);

        ASSERT_TRUE(values
                        .emplace(std::make_pair(
                            i + 1, sme::msp::string(std::to_string(i + 1), str_alc)))
                        .second);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(0, sme::msp::string(std::to_string(0), str_alc))));
}

TEST(MemorySpaceAllocTest, TestStringUnorderedMapAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::unordered_map<sme::msp::string, sme::msp::string>::allocator_type malc{
        *ms_ptr};

    sme::msp::unordered_map<sme::msp::string, sme::msp::string> values{malc};

    values = {{sme::msp::string("1", str_alc), sme::msp::string("10", str_alc)},
              {sme::msp::string("2", str_alc), sme::msp::string("20", str_alc)}};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i += 2) {
        sme::msp::string key1(kSomeChars + std::to_string(i), str_alc);
        sme::msp::string value1(kSomeChars + std::to_string(i * 10), str_alc);

        auto item1 = std::make_pair(key1, value1);
        ASSERT_TRUE(values.insert(item1).second);

        sme::msp::string key2(kSomeChars + std::to_string(i + 1), str_alc);
        sme::msp::string value2(kSomeChars + std::to_string((i + 1) * 10), str_alc);

        ASSERT_TRUE(values.emplace(std::make_pair(key2, value2)).second);
    }

    for (auto i = 3U; i < some_size; i++) {
        const sme::msp::string key(kSomeChars + std::to_string(i), str_alc);
        const sme::msp::string value(kSomeChars + std::to_string(i * 10), str_alc);

        auto found_iter = values.find(key);
        ASSERT_TRUE(found_iter != values.cend());
        ASSERT_EQ(found_iter->second, value);
    }

    ASSERT_NO_THROW(values.clear());

    ASSERT_NO_THROW(
        values.emplace(std::make_pair(sme::msp::string{std::to_string(0), str_alc},
                                      sme::msp::string{std::to_string(0), str_alc})));
}

TEST(MemorySpaceAllocTest, TestSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::set<uint64_t>::allocator_type malc{*ms_ptr};

    sme::msp::set<uint64_t> values(
        {
            1,
            2,
        },
        malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i++)
        ASSERT_TRUE(values.emplace(i).second);

    for (auto i = 1U; i < some_size; i++)
        ASSERT_TRUE(values.find(i) != values.cend());

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestStringSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::set<sme::msp::string>::allocator_type malc{*ms_ptr};

    sme::msp::set<sme::msp::string> values(
        {{sme::msp::string("1", str_alc)}, {sme::msp::string("2", str_alc)}}, malc);

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i++)
        ASSERT_TRUE(values.emplace(sme::msp::string{std::to_string(i), str_alc}).second);

    for (auto i = 1U; i < some_size; i++)
        ASSERT_TRUE(values.find(sme::msp::string{std::to_string(i), str_alc}) !=
                    values.cend());

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestUnorderedSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::unordered_set<uint64_t>::allocator_type malc{*ms_ptr};

    sme::msp::unordered_set<uint64_t> values{malc};
    values = {1, 2};

    constexpr auto some_size{123U};

    for (auto i = 3U; i < some_size; i++)
        ASSERT_TRUE(values.emplace(i).second);

    for (auto i = 1U; i < some_size; i++)
        ASSERT_TRUE(values.find(i) != values.cend());

    ASSERT_NO_THROW(values.clear());
}

TEST(MemorySpaceAllocTest, TestStringUnorderedSetAllocation)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<sme::MemorySpace> ms_ptr{&mem_space};

    sme::msp::string::allocator_type str_alc{*ms_ptr};
    sme::msp::unordered_set<sme::msp::string>::allocator_type malc{*ms_ptr};

    sme::msp::unordered_set<sme::msp::string> values{malc};
    values = {sme::msp::string("1", str_alc), sme::msp::string("2", str_alc)};

    ASSERT_NO_THROW(values.clear());

    constexpr auto some_size{123U};

    for (auto i = 0U; i < some_size; i++) {
        sme::msp::string s{i, 'X', str_alc};
        ASSERT_TRUE(values.emplace(s).second);
    }

    for (auto i = 0U; i < some_size; i++) {
        sme::msp::string s{i, 'X', str_alc};
        ASSERT_TRUE(values.find(s) != values.cend());
    }
}
