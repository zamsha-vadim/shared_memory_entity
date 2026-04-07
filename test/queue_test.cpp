#include <vector>

#include <gtest/gtest.h>

#include <sme/internal/queue.h>

namespace {

template <typename T>
class Item {
   public:
    using ItemPointer = sme::Pointer<Item<T>>;

   public:
    Item() = default;
    Item(T value) noexcept : value_{std::move(value)} {}

    auto GetValue() const noexcept -> const T& { return value_; }
    auto GetValue() noexcept -> T& { return value_; }
    void SetValue(T&& value) { value_ = std::move(value); }

    auto GetNext() const noexcept -> ItemPointer { return next_; }
    void SetNext(ItemPointer next) noexcept { next_ = std::move(next); }

   private:
    ItemPointer next_;
    T value_{};
};

using IntValue = Item<int>;
using IntValuePointer = sme::Pointer<IntValue>;

}  // namespace

TEST(QueueTest, TestWriteReadOneItem)
{
    sme::Queue<IntValue> queue;

    ASSERT_TRUE(queue.IsEmpty());

    IntValue v{1};
    IntValuePointer pv{&v};

    queue.Write(pv);

    ASSERT_FALSE(queue.IsEmpty());

    IntValuePointer chk_pv = queue.Read();

    ASSERT_TRUE(chk_pv != nullptr);
    ASSERT_EQ(pv, chk_pv);
    ASSERT_TRUE(queue.IsEmpty());
}

TEST(QueueTest, TestWriteSomeAndReadSequentiallySomeItems)
{
    sme::Queue<IntValue> queue;

    constexpr int kTryCount{10};

    for (int i = 0; i < kTryCount; i++) {
        IntValue v{i};
        IntValuePointer pv{&v};

        queue.Write(pv);
        IntValuePointer chk_pv = queue.Read();

        ASSERT_EQ(pv, chk_pv);
        ASSERT_EQ(pv->GetValue(), chk_pv->GetValue());
    }
}

TEST(QueueTest, TestWriteSequentiallySomeItemsAndReadSequentially)
{
    sme::Queue<IntValue> queue;

    for (int k = 0; k < 3; k++) {
        constexpr int kItemCount{3};
        std::array<IntValue, kItemCount> values;
        std::array<IntValuePointer, kItemCount> value_ptrs;

        for (int i = 0; i < kItemCount; i++) {
            values[i] = IntValue{i};
            value_ptrs[i] = (IntValuePointer(&values[i]));

            queue.Write(value_ptrs[i]);
        }

        EXPECT_FALSE(queue.IsEmpty());

        std::vector<IntValuePointer> chk_value_ptrs;

        IntValuePointer ptr;
        while ((ptr = queue.Read()) != nullptr) {
            chk_value_ptrs.push_back(ptr);
        }

        EXPECT_TRUE(queue.IsEmpty());

        EXPECT_EQ(chk_value_ptrs.size(), value_ptrs.size());

        for (int i = 0; i < kItemCount; i++) {
            ASSERT_EQ(value_ptrs[i], chk_value_ptrs[i]);
            ASSERT_EQ(chk_value_ptrs[i]->GetValue(), value_ptrs[i]->GetValue());
        }
    }
}
