#include <gtest/gtest.h>

#include <sme/internal/sync_queue.h>

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

TEST(SyncQueueTest, Test)
{
    sme::SynchronizedQueue<IntValue> queue{sme::SyncronizationType::kSpin};

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
