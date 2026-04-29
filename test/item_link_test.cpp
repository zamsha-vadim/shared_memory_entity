#include <string>

#include <gtest/gtest.h>

#include <sme/internal/item_link.h>

struct Foo {
    int x;
    double y;
};

struct Bar {
    std::string s;
    double y;
};

struct Baz {
    Foo foo;
    Bar bar;
};

TEST(ItemLinkTest, TestExtractZeroOffset)
{
    sme::ObjectOffset ofs_ptr{0};

    EXPECT_EQ(sme::ExtractOffset(ofs_ptr), 0);
}

TEST(ItemLinkTest, TestGetObjectOffsetForSelf)
{
    Baz baz{};

    auto ofs_ptr = sme::GetObjectOffset(&baz, &baz);

    EXPECT_EQ(ofs_ptr, 0);
}

TEST(ItemLinkTest, TestGetObjectOffsetForPositiveOffset)
{
    Baz baz{};

    auto ofs_ptr = sme::GetObjectOffset(&baz.foo, &baz.bar);

    EXPECT_EQ(ofs_ptr, offsetof(Baz, bar));
}

TEST(ItemLinkTest, TestGetObjectOffsetForNegativeOffset)
{
    Baz baz{};

    auto ofs_ptr = sme::GetObjectOffset(&baz.bar, &baz);

    EXPECT_EQ((ofs_ptr & ~sme::GetNegativeOffsetFlag()), offsetof(Baz, bar));
}

TEST(ItemLinkTest, TestGetObjectAddressForSelf)
{
    Baz baz{};
    auto ofs_ptr = sme::GetObjectOffset(&baz, &baz);

    auto* chk_addr = sme::GetObjectAddress<Baz, Baz>(&baz, ofs_ptr);

    EXPECT_EQ(&baz, chk_addr);
}

TEST(ItemLinkTest, TestGetObjectAddressForOffsetZero)
{
    Baz baz{};
    auto ofs_ptr = sme::GetObjectOffset(&baz, &baz);

    auto* chk_addr = sme::GetObjectAddress<Baz, Baz>(&baz, ofs_ptr);

    EXPECT_EQ(&baz, chk_addr);
}

TEST(ItemLinkTest, TestGetObjectAddressForPositiveOffset)
{
    Baz baz{};
    auto ofs_ptr = sme::GetObjectOffset(&baz.foo, &baz.bar);

    auto* chk_addr = sme::GetObjectAddress<Foo, Bar>(&baz.foo, ofs_ptr);

    EXPECT_EQ(&baz.bar, chk_addr);
}

TEST(ItemLinkTest, TestGetObjectAddressForNegativeOffset)
{
    Baz baz{};
    auto ofs_ptr = sme::GetObjectOffset(&baz.bar, &baz.foo);

    auto* chk_addr = sme::GetObjectAddress<Bar, Foo>(&baz.bar, ofs_ptr);

    EXPECT_EQ(&baz.foo, chk_addr);
}

TEST(ItemLinkTest, TestUseCounterWithPositiveAddressOffset)
{
    Baz baz{};
    auto ofs_ptr = sme::GetObjectOffset(&baz.foo, &baz.bar);
    auto ofs = sme::ExtractOffset(ofs_ptr);

    auto counter1 = sme::ExtractUseCounter(ofs_ptr);
    ASSERT_EQ(counter1, 0);

    constexpr int16_t some_value1{1};
    constexpr int16_t some_value2{24999};
    constexpr int16_t some_value3{100};

    auto counter2 = sme::IncreaseUseCounter(ofs_ptr, some_value1);
    ASSERT_EQ(counter2, sme::ExtractUseCounter(ofs_ptr));
    ASSERT_EQ(counter2, counter1 + some_value1);
    ASSERT_EQ(ofs, sme::ExtractOffset(ofs_ptr));

    auto counter3 = sme::IncreaseUseCounter(ofs_ptr, some_value2);
    ASSERT_EQ(counter3, sme::ExtractUseCounter(ofs_ptr));
    ASSERT_EQ(counter3, counter2 + some_value2);
    ASSERT_EQ(ofs, sme::ExtractOffset(ofs_ptr));

    auto counter4 = sme::IncreaseUseCounter(ofs_ptr, some_value3);
    ASSERT_EQ(counter4, sme::ExtractUseCounter(ofs_ptr));
    ASSERT_EQ(counter4, counter3 + some_value3);
    ASSERT_EQ(ofs, sme::ExtractOffset(ofs_ptr));
}

TEST(ItemLinkTest, TestUseCounterWithNegativeAddressOffset)
{
    Baz baz{};
    auto ofs_ptr = sme::GetObjectOffset(&baz.bar, &baz.foo);
    auto ofs = sme::ExtractOffset(ofs_ptr);

    auto counter1 = sme::ExtractUseCounter(ofs_ptr);
    ASSERT_EQ(counter1, 0);

    constexpr int16_t some_value1{1};
    constexpr int16_t some_value2{24999};
    constexpr int16_t some_value3{100};

    auto counter2 = sme::IncreaseUseCounter(ofs_ptr, some_value1);
    ASSERT_EQ(counter2, sme::ExtractUseCounter(ofs_ptr));
    ASSERT_EQ(counter2, counter1 + some_value1);
    ASSERT_EQ(ofs, sme::ExtractOffset(ofs_ptr));

    auto counter3 = sme::IncreaseUseCounter(ofs_ptr, some_value2);
    ASSERT_EQ(counter3, sme::ExtractUseCounter(ofs_ptr));
    ASSERT_EQ(counter3, counter2 + some_value2);
    ASSERT_EQ(ofs, sme::ExtractOffset(ofs_ptr));

    auto counter4 = sme::IncreaseUseCounter(ofs_ptr, some_value3);
    ASSERT_EQ(counter4, sme::ExtractUseCounter(ofs_ptr));
    ASSERT_EQ(counter4, counter3 + some_value3);
    ASSERT_EQ(ofs, sme::ExtractOffset(ofs_ptr));
}
