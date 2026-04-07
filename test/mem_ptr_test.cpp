#include <alloca.h>
#include <array>
#include <cstddef>
#include <cstring>

#include <gtest/gtest.h>

#include "sme/mem_ptr.h"

struct Foo {
    int val1{};
    double val2{};
};

using SomeType = int;

constexpr int kSomeValue{123};
constexpr int kAnotherValue{kSomeValue * 3};

constexpr size_t kSomeSize{10};

template <typename T, size_t SizeValue>
void Fill(std::array<T, SizeValue>& values, T start_value = kSomeValue)
{
    auto counter = start_value;
    for (auto& value : values)
        value = counter++;
}

TEST(PointerTest, TestDefaultConstructor)
{
    const sme::Pointer<Foo> ptr;

    EXPECT_TRUE(ptr.IsNull());
    EXPECT_TRUE(!ptr);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr, nullptr);

    EXPECT_ANY_THROW(ptr->val1);
}

TEST(PointerTest, TestStandardConstructor)
{
    int value{kSomeValue};

    const sme::Pointer<int> ptr(&value);

    EXPECT_FALSE(ptr.IsNull());
    EXPECT_FALSE(!ptr);
    EXPECT_TRUE(ptr);
    EXPECT_NE(ptr, nullptr);
    ASSERT_EQ(ptr, &value);

    EXPECT_EQ(*ptr, kSomeValue);
}

TEST(PointerTest, TestCopyConstructor)
{
    int value{kSomeValue};

    const sme::Pointer<int> ptr1(&value);
    const sme::Pointer<int> ptr2(ptr1);  // NOLINT

    ASSERT_EQ(ptr1, ptr2);
    ASSERT_EQ(ptr2, &value);
    ASSERT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestTypedCopyConstructor)
{
    int value{kSomeValue};

    const sme::Pointer<void> ptr1(&value);
    const sme::Pointer<int> ptr2(ptr1);  // NOLINT

    EXPECT_EQ(ptr1, ptr2);
    EXPECT_EQ(ptr2, &value);
    EXPECT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestMoveConstructor)
{
    int value{kSomeValue};

    sme::Pointer<int> ptr1{&value};
    const sme::Pointer<int> ptr2{std::move(ptr1)};

    EXPECT_EQ(ptr2, &value);
    EXPECT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestTypedMoveConstructor)
{
    int value{kSomeValue};

    sme::Pointer<void> ptr1(&value);
    sme::Pointer<int> ptr2{std::move(ptr1)};

    EXPECT_EQ(ptr2, &value);
    EXPECT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestCopyOperator)
{
    int value{kSomeValue};

    sme::Pointer<int> ptr1(&value);
    sme::Pointer<int> ptr2;

    ptr2 = ptr1;

    ASSERT_EQ(ptr1, ptr2);
    ASSERT_EQ(ptr2, &value);
    ASSERT_EQ(*ptr2, kSomeValue);

    int another_value{kAnotherValue};
    ptr1 = &another_value;

    ASSERT_NE(ptr1, ptr2);
    ASSERT_NE(ptr1.GetAddress(), &value);
    ASSERT_EQ(ptr1, &another_value);
    ASSERT_EQ(*ptr1, kAnotherValue);

    ASSERT_EQ(ptr2, &value);
    ASSERT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestTypedCopyOperator)
{
    int value{kSomeValue};

    const sme::Pointer<void> ptr1(&value);
    sme::Pointer<int> ptr2;

    ptr2 = ptr1;

    EXPECT_EQ(ptr1, ptr2);
    EXPECT_EQ(ptr2, &value);
    EXPECT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestMoveOperator)
{
    int value{kSomeValue};

    sme::Pointer<int> ptr;

    ptr = sme::Pointer<int>(&value);

    ASSERT_EQ(ptr, &value);
    ASSERT_EQ(*ptr, kSomeValue);
}

TEST(PointerTest, TestTypedMoveOperator)
{
    int value{kSomeValue};

    sme::Pointer<void> ptr1(&value);
    sme::Pointer<int> ptr2;

    ptr2 = std::move(ptr1);

    EXPECT_EQ(ptr1, nullptr);
    EXPECT_EQ(ptr2, &value);
    EXPECT_EQ(*ptr2, kSomeValue);
}

TEST(PointerTest, TestReset)
{
    int value{kSomeValue};

    sme::Pointer<int> ptr(&value);
    ptr.Reset();

    EXPECT_TRUE(ptr.IsNull());
    EXPECT_TRUE(!ptr);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST(PointerTest, TestCompare)
{
    int value{kSomeValue};

    const sme::Pointer<int> ptr1(&value);
    sme::Pointer<int> ptr2;
    sme::Pointer<const int> ptr3;

    EXPECT_NE(ptr1, ptr2);
    EXPECT_NE(ptr1, ptr3);

    EXPECT_EQ(ptr2, ptr3);

    ptr2 = ptr1;

    EXPECT_EQ(ptr1, ptr2);
    EXPECT_NE(ptr2, ptr3);

    ptr3 = ptr1;

    EXPECT_EQ(ptr1, ptr3);
    EXPECT_EQ(ptr2, ptr3);
}

TEST(PointerTest, TestCompareTypedAddresses)
{
    int value{kSomeValue};

    const sme::Pointer<int> ptr1(&value);
    sme::Pointer<void> ptr2;

    EXPECT_NE(ptr1, ptr2);

    ptr2 = static_cast<void*>(&value);
    EXPECT_EQ(ptr1, ptr2);

    void* void_p = ptr1.GetAddress();
    EXPECT_EQ(ptr1, void_p);
    EXPECT_EQ(void_p, ptr1);
    EXPECT_EQ(void_p, ptr2);
}

TEST(PointerTest, TestAddOffset)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const size_t some_pos{1};
    const std::ptrdiff_t some_ofs{2};

    sme::Pointer<int> ptr{&values[some_pos]};

    ptr += some_ofs;

    ASSERT_EQ(*ptr, values[some_pos + some_ofs]);
}

TEST(PointerTest, TestPrefixIncrement)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const size_t some_pos{2};

    sme::Pointer<int> ptr{&values[some_pos]};

    auto curr_ptr = ++ptr;
    EXPECT_EQ(*curr_ptr, values[some_pos + 1]);
    EXPECT_EQ(*ptr, values[some_pos + 1]);
}

TEST(PointerTest, TestPostfixIncrement)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const size_t some_pos{2};

    sme::Pointer<int> ptr{&values[some_pos]};

    auto prev_ptr = ptr++;
    EXPECT_EQ(*prev_ptr, values[some_pos]);
    EXPECT_EQ(*ptr, values[some_pos + 1]);
}

TEST(PointerTest, TestSubstractOffset)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const size_t some_pos{5};
    const ptrdiff_t some_ofs{2};

    sme::Pointer<int> ptr{&values[some_pos]};

    ptr -= some_ofs;
    ASSERT_EQ(*ptr, values[some_pos - some_ofs]);
}

TEST(PointerTest, TestPrefixDecrement)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const size_t some_pos{2};

    sme::Pointer<int> ptr{&values[some_pos]};

    auto curr_ptr = --ptr;
    EXPECT_EQ(*curr_ptr, values[some_pos - 1]);
    EXPECT_EQ(*ptr, values[some_pos - 1]);
}

TEST(PointerTest, TestPostfixDecrement)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const size_t some_pos{2};

    sme::Pointer<int> ptr{&values[some_pos]};

    auto prev_ptr = ptr--;
    EXPECT_EQ(*prev_ptr, values[some_pos]);
    EXPECT_EQ(*ptr, values[some_pos - 1]);
}

TEST(PointerTest, TestSingleValue)
{
    int value{kSomeValue};

    const sme::Pointer<int> ptr(&value);

    ASSERT_EQ(*ptr, value);
    ASSERT_EQ(ptr[0], value);

    const int new_value = value * 2;
    *ptr = new_value;
    ASSERT_EQ(*ptr, new_value);

    value = kAnotherValue;
    ASSERT_EQ(ptr[0], value);
    ASSERT_EQ(*ptr, kAnotherValue);
}

TEST(PointerTest, TestArrayValue)
{
    std::array<int, kSomeSize> values{};
    Fill(values);

    const sme::Pointer<int> ptr(values.data());

    ASSERT_EQ(ptr, values.data());

    for (size_t i = 0; i < values.size(); i++)
        ASSERT_EQ(ptr[i], values[i]);  // NOLINT
}

TEST(PointerTest, TestValueLocationMoving)
{
    struct DummyStruct {
        int value{};
        sme::Pointer<int> ptr;
    };

    DummyStruct data1{};
    data1.value = kSomeValue;
    data1.ptr = &data1.value;

    void* mem2 = alloca(sizeof(DummyStruct));
    std::memcpy(mem2, &data1, sizeof(DummyStruct));  // NOLINT

    DummyStruct& data2 = *reinterpret_cast<DummyStruct*>(mem2);  // NOLINT

    ASSERT_NE(data1.ptr, data2.ptr);
    ASSERT_EQ(*data2.ptr, data2.value);
    ASSERT_EQ(data2.value, kSomeValue);
}

TEST(PointerTest, TestEqualPointerWithSameType)
{
    int value{kSomeValue};

    sme::Pointer<int> ptr1(&value);
    sme::Pointer<const int> cptr1(&value);

    const sme::Pointer<int> ptr2(&value);
    const sme::Pointer<const int> cptr2(&value);

    ASSERT_TRUE(ptr1 == ptr2);
    ASSERT_TRUE(ptr1 == cptr2);

    ASSERT_TRUE(cptr1 == ptr2);
    ASSERT_TRUE(cptr1 == cptr2);
}

TEST(PointerTest, TestEqualPointersWithDifferentTypes)
{
    int value{kSomeValue};

    char* rp1 = reinterpret_cast<char*>(&value);
    const char* crp1 = rp1;

    sme::Pointer<int> p2(&value);
    sme::Pointer<const int> cp2(&value);
    const sme::Pointer<const int> ccp2(&value);

    ASSERT_EQ(rp1, p2);
    ASSERT_TRUE(rp1 == p2);

    ASSERT_EQ(rp1, p2);
    ASSERT_EQ(rp1, cp2);
    ASSERT_TRUE(rp1 == ccp2);

    ASSERT_EQ(crp1, p2);
    ASSERT_TRUE(crp1 == cp2);
    ASSERT_TRUE(crp1 == ccp2);

    ASSERT_TRUE(p2 == rp1);
    ASSERT_TRUE(p2 == crp1);

    ASSERT_TRUE(cp2 == rp1);
    ASSERT_TRUE(cp2 == crp1);

    ASSERT_TRUE(ccp2 == rp1);
    ASSERT_TRUE(ccp2 == crp1);

    ASSERT_TRUE(p2 == cp2);
    ASSERT_TRUE(cp2 == p2);
}

TEST(PointerTest, TestNotEqualPointersWithSameType)
{
    int value1{kSomeValue};
    int value2{kSomeValue};

    sme::Pointer<int> ptr1(&value1);
    sme::Pointer<const int> cptr1(&value1);

    const sme::Pointer<int> ptr2(&value2);
    const sme::Pointer<const int> cptr2(&value2);

    ASSERT_TRUE(ptr1 != ptr2);
    ASSERT_TRUE(ptr1 != cptr2);

    ASSERT_TRUE(cptr1 != ptr2);
    ASSERT_TRUE(cptr1 != cptr2);
}

TEST(PointerTest, TestiNotEqualPointersWithDifferentTypes)
{
    int value1{kSomeValue};
    int value2{kSomeValue};

    char* rp1 = reinterpret_cast<char*>(&value1);
    const char* crp1 = rp1;

    sme::Pointer<int> p2(&value2);
    sme::Pointer<const int> cp2(&value2);
    const sme::Pointer<const int> ccp2(&value2);

    ASSERT_TRUE(rp1 != p2);
    ASSERT_TRUE(rp1 != cp2);
    ASSERT_TRUE(rp1 != ccp2);

    ASSERT_TRUE(crp1 != p2);
    ASSERT_TRUE(crp1 != cp2);
    ASSERT_TRUE(crp1 != ccp2);

    ASSERT_TRUE(p2 != rp1);
    ASSERT_TRUE(p2 != crp1);

    ASSERT_TRUE(cp2 != rp1);
    ASSERT_TRUE(cp2 != crp1);

    ASSERT_TRUE(ccp2 != rp1);
    ASSERT_TRUE(ccp2 != crp1);
}

TEST(PointerTest, TestLessPointersWithSameType)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    sme::Pointer<int> ptr1(&values[0]);
    sme::Pointer<const int> cptr1(&values[0]);

    const sme::Pointer<int> ptr2(&values[1]);
    const sme::Pointer<const int> cptr2(&values[1]);

    ASSERT_TRUE(ptr1 < ptr2);
    ASSERT_TRUE(ptr1 < cptr2);

    ASSERT_TRUE(cptr1 < ptr2);
    ASSERT_TRUE(cptr1 < cptr2);
}

TEST(PointerTest, TestLessPointersWithDifferentTypes)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    char* rp1 = reinterpret_cast<char*>(&values[0]);
    const char* crp1 = rp1;

    sme::Pointer<int> p2(&values[1]);
    sme::Pointer<const int> cp2(&values[1]);
    const sme::Pointer<const int> ccp2(&values[1]);

    ASSERT_TRUE(rp1 < p2);
    ASSERT_TRUE(rp1 < cp2);
    ASSERT_TRUE(rp1 < ccp2);

    ASSERT_TRUE(crp1 < p2);
    ASSERT_TRUE(crp1 < cp2);
    ASSERT_TRUE(crp1 < ccp2);
}

TEST(PointerTest, TestLessEqualPointersWithSameType)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    sme::Pointer<int> ptr1(&values[0]);
    sme::Pointer<const int> cptr1(&values[0]);

    const sme::Pointer<int> ptr2(&values[1]);
    const sme::Pointer<const int> cptr2(&values[1]);

    ASSERT_TRUE(ptr1 <= ptr2);
    ASSERT_TRUE(ptr1 <= cptr2);

    ASSERT_TRUE(cptr1 <= ptr2);
    ASSERT_TRUE(cptr1 <= cptr2);
}

TEST(PointerTest, TestLessEqualPointersWithDifferentTypes)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    char* rp1 = reinterpret_cast<char*>(&values[0]);
    const char* crp1 = rp1;

    sme::Pointer<int> p2(&values[1]);
    sme::Pointer<const int> cp2(&values[1]);
    const sme::Pointer<const int> ccp2(&values[1]);

    ASSERT_TRUE(rp1 <= p2);
    ASSERT_TRUE(rp1 <= cp2);
    ASSERT_TRUE(rp1 <= ccp2);

    ASSERT_TRUE(crp1 <= p2);
    ASSERT_TRUE(crp1 <= cp2);
    ASSERT_TRUE(crp1 <= ccp2);
}

TEST(PointerTest, TestGreaterPointersWithSameType)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    sme::Pointer<int> ptr1(&values[1]);
    sme::Pointer<const int> cptr1(&values[1]);

    const sme::Pointer<int> ptr2(&values[0]);
    const sme::Pointer<const int> cptr2(&values[0]);

    ASSERT_TRUE(ptr1 > ptr2);
    ASSERT_TRUE(ptr1 > cptr2);

    ASSERT_TRUE(cptr1 > ptr2);
    ASSERT_TRUE(cptr1 > cptr2);
}

TEST(PointerTest, TestGreaterPointersWithDifferentTypes)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    char* rp1 = reinterpret_cast<char*>(&values[1]);
    const char* crp1 = rp1;

    sme::Pointer<int> p2(&values[0]);
    sme::Pointer<const int> cp2(&values[0]);
    const sme::Pointer<const int> ccp2(&values[0]);

    ASSERT_TRUE(rp1 > p2);
    ASSERT_TRUE(rp1 > cp2);
    ASSERT_TRUE(rp1 > ccp2);

    ASSERT_TRUE(crp1 > p2);
    ASSERT_TRUE(crp1 > cp2);
    ASSERT_TRUE(crp1 > ccp2);
}

TEST(PointerTest, TestGreaterEqualPointersWithSameType)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    sme::Pointer<int> ptr1(&values[1]);
    sme::Pointer<const int> cptr1(&values[1]);

    const sme::Pointer<int> ptr2(&values[0]);
    const sme::Pointer<const int> cptr2(&values[0]);

    ASSERT_TRUE(ptr1 >= ptr2);
    ASSERT_TRUE(ptr1 >= cptr2);

    ASSERT_TRUE(cptr1 >= ptr2);
    ASSERT_TRUE(cptr1 >= cptr2);
}

TEST(PointerTest, TestGreaterEqualPointersWithDifferentTypes)
{
    std::array<int, 2> values = {kSomeValue, kSomeValue};

    char* rp1 = reinterpret_cast<char*>(&values[1]);
    const char* crp1 = rp1;

    sme::Pointer<int> p2(&values[0]);
    sme::Pointer<const int> cp2(&values[0]);
    const sme::Pointer<const int> ccp2(&values[0]);

    ASSERT_TRUE(rp1 >= p2);
    ASSERT_TRUE(rp1 >= cp2);
    ASSERT_TRUE(rp1 >= ccp2);

    ASSERT_TRUE(crp1 >= p2);
    ASSERT_TRUE(crp1 >= cp2);
    ASSERT_TRUE(crp1 >= ccp2);
}
