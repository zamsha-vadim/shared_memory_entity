#include <algorithm>
#include <atomic>
#include <deque>
#include <list>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sme/internal/lf_queue.h>

namespace {

template <typename T>
struct alignas(16) Item {
    using CounterType = sme::ReferenceCounter<>::ValueType;

    auto AddReference(CounterType count = 1) const noexcept -> CounterType
    {
        auto value = counter_.Increment(count);
        assert(value != std::numeric_limits<CounterType>::min());
        return value;
    }

    auto RemoveReference() const noexcept -> CounterType
    {
        auto value = counter_.Decrement();
        assert(value != std::numeric_limits<CounterType>::max());
        return value;
    }

    auto GetReferenceCount() const noexcept
    {
        return counter_.GetValue();
    }

    auto GetItemDescriptor() const noexcept -> const sme::ItemDescriptor&
    {
        return item_descriptor_;
    }

    auto GetItemDescriptor() noexcept -> sme::ItemDescriptor& { return item_descriptor_; }

    auto GetValue() const noexcept { return value_; }
    auto SetValue(T value) noexcept { value_ = std::move(value); }

   private:
    mutable sme::ItemDescriptor item_descriptor_{};
    mutable sme::ReferenceCounter<> counter_{1};

    T value_{};
};

}  // namespace

TEST(LockFreeQueueTest, TestReadingWaitWithTimeout)
{
    sme::LockFreeQueue<Item<int>> queue;

    const auto timeout{std::chrono::milliseconds(200)};

    auto begin_time = std::chrono::steady_clock::now();

    auto res = queue.WaitForReading(timeout);

    auto elapsed_time = std::chrono::steady_clock::now() - begin_time;
    auto real_timeout =
        timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time);

    ASSERT_EQ(res, sme::QueueResult::kTimeout);
    ASSERT_LE(0, real_timeout.count());
    ASSERT_LE(real_timeout.count(), timeout.count() + 20);
}

TEST(LockFreeQueueTest, TestReadingWaitWithoutTimeout)
{
    sme::LockFreeQueue<Item<int>> queue;

    const auto kNoTimeout{std::chrono::milliseconds(0)};

    auto begin_time = std::chrono::steady_clock::now();

    auto res = queue.WaitForReading(kNoTimeout);

    auto elapsed_time = std::chrono::steady_clock::now() - begin_time;
    auto real_timeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time) - kNoTimeout;

    ASSERT_EQ(res, sme::QueueResult::kTimeout);
    ASSERT_LE(real_timeout.count(), 20);
}

TEST(LockFreeQueueTest, TestWriteAndReadOneItem)
{
    sme::LockFreeQueue<Item<int>> queue;

    ASSERT_TRUE(queue.IsEmpty());

    auto [res1, empty_item] = queue.Read();
    ASSERT_TRUE(empty_item == nullptr);

    Item<int> wr_item{};
    wr_item.SetValue(1);

    res1 = queue.Write(&wr_item);
    ASSERT_TRUE(res1 == sme::QueueResult::kSuccessful);
    ASSERT_FALSE(queue.IsEmpty());
    ASSERT_EQ(wr_item.GetReferenceCount(), 3);

    auto [res2, rd_item2] = queue.Read();

    ASSERT_TRUE(res2 == sme::QueueResult::kSuccessful);
    ASSERT_TRUE(rd_item2 != nullptr);
    ASSERT_TRUE(rd_item2 == &wr_item);
    ASSERT_EQ(rd_item2->GetReferenceCount(), 1);

    ASSERT_TRUE(queue.IsEmpty());

    auto[res3, rd_item3] = queue.Read();

    ASSERT_TRUE(rd_item3 == nullptr);
    ASSERT_TRUE(queue.IsEmpty());
}

TEST(LockFreeQueueTest, TestReadWithTimeout)
{
    const auto noTimeout{std::chrono::milliseconds(0)};
    const auto timeout{std::chrono::milliseconds(200)};

    sme::LockFreeQueue<Item<int>> queue;

    auto [res0, rd_item0] = queue.Read(noTimeout);
    ASSERT_TRUE(res0 == sme::QueueResult::kTimeout);
    ASSERT_TRUE(rd_item0 == nullptr);

    auto [res1, rd_item1] = queue.Read(timeout);
    ASSERT_TRUE(res1 == sme::QueueResult::kTimeout);
    ASSERT_TRUE(rd_item1 == nullptr);

    Item<int> wr_item{};
    wr_item.SetValue(1);

    auto wr_res = queue.Write(&wr_item);
    ASSERT_TRUE(wr_res == sme::QueueResult::kSuccessful);

    auto [res2, rd_item2] = queue.Read(timeout);

    ASSERT_TRUE(res2 == sme::QueueResult::kSuccessful);
    ASSERT_TRUE(rd_item2 != nullptr);
    ASSERT_TRUE(rd_item2 == &wr_item);

    auto[res3, rd_item3] = queue.Read(timeout);

    ASSERT_TRUE(res3 == sme::QueueResult::kTimeout);
    ASSERT_TRUE(rd_item3 == nullptr);
}

TEST(LockFreeQueueTest, TestWriteAndReadSequentiallySomeItems)
{
    sme::LockFreeQueue<Item<int>> queue;

    constexpr int kTryCount{10};

    for (int i = 0; i < kTryCount; i++) {
        Item<int> wr_item{};
        wr_item.SetValue(i);

        ASSERT_TRUE(queue.IsEmpty());

        auto wr_res = queue.Write(&wr_item);
        ASSERT_TRUE(wr_res == sme::QueueResult::kSuccessful);
        ASSERT_FALSE(queue.IsEmpty());
        ASSERT_EQ(wr_item.GetReferenceCount(), 3);

        auto [rd_res, rd_item] = queue.Read();
        ASSERT_TRUE(rd_res == sme::QueueResult::kSuccessful);
        ASSERT_TRUE(queue.IsEmpty());

        ASSERT_TRUE(rd_item != nullptr) << "Item: " << i;
        ASSERT_TRUE(rd_item == &wr_item) << "Item: " << i;
        ASSERT_EQ(rd_item->GetReferenceCount(), 1) << "Item: " << i;
        ASSERT_TRUE(rd_item->GetValue() == wr_item.GetValue()) << "Item: " << i;
    }
}

TEST(LockFreeQueueTest, TestWriteSequentiallySomeItemsAndReadSequentially)
{
    sme::LockFreeQueue<Item<int>> queue;

    constexpr int kTryCount{10};

    for (int k = 0; k < kTryCount; k++) {
        constexpr int kItemCount{100};
        std::array<Item<int>, kItemCount> values;

        for (int i = 0; i < kItemCount; i++) {
            values[i].SetValue(i);

            auto res = queue.Write(&values[i]);
            ASSERT_TRUE(res == sme::QueueResult::kSuccessful);
        }

        EXPECT_FALSE(queue.IsEmpty());

        std::deque<Item<int>*> chk_values;

        std::pair<sme::QueueResult, Item<int>*> rd_res{};
        do {
            rd_res = queue.Read();
            if (rd_res.second != nullptr) 
                chk_values.push_back(rd_res.second);
        } while (rd_res.first == sme::QueueResult::kSuccessful);

        EXPECT_TRUE(queue.IsEmpty());

        EXPECT_EQ(chk_values.size(), values.size());

        for (int i = 0; i < kItemCount; i++) {
            ASSERT_EQ(&values[i], chk_values[i]);
            ASSERT_EQ(values[i].GetValue(), chk_values[i]->GetValue());
            //std::cout << values[i].GetValue() << std::endl;
        }
    }
}

TEST(LockFreeQueueTest, TestDisabled)
{
    sme::LockFreeQueue<Item<int>> queue;

    ASSERT_FALSE(queue.IsDisabled());

    queue.Disable();
    ASSERT_TRUE(queue.IsDisabled());

    queue.Disable(false);
    ASSERT_FALSE(queue.IsDisabled());
}

TEST(LockFreeQueueTest, TestWriteOnDisabled)
{
    sme::LockFreeQueue<Item<int>> queue;

    queue.Disable();

    Item<int> wr_item{};
    wr_item.SetValue(1);

    auto wr_res = queue.Write(&wr_item);
    ASSERT_TRUE(wr_res == sme::QueueResult::kRejected);

    queue.Disable(false);

    wr_res = queue.Write(&wr_item);
    ASSERT_TRUE(wr_res == sme::QueueResult::kSuccessful);
}

TEST(LockFreeQueueTest, TestReadOnDisabled)
{
    const auto timeout{std::chrono::milliseconds(20)};

    sme::LockFreeQueue<Item<int>> queue;

    Item<int> wr_item{};
    wr_item.SetValue(1);

    auto wr_res = queue.Write(&wr_item);
    ASSERT_TRUE(wr_res == sme::QueueResult::kSuccessful);

    queue.Disable();

    auto [res1, rd_item1] = queue.Read(timeout);
    ASSERT_TRUE(res1 == sme::QueueResult::kRejected);
    ASSERT_TRUE(rd_item1 == nullptr);

    queue.Disable(false);

    auto [res2, rd_item2] = queue.Read(timeout);
    ASSERT_TRUE(res2 == sme::QueueResult::kSuccessful);
    ASSERT_TRUE(rd_item2 == &wr_item);
}

TEST(LockFreeQueueTest, TestDestructor)
{
    std::vector<Item<int>> items(10);
    std::vector<size_t> item_refs(items.size());

    auto pos{0u};

    for (auto& item : items)
        item_refs[pos++] = item.GetReferenceCount();

    {
        sme::LockFreeQueue<Item<int>> queue;

        for (auto& item : items) {
            auto res = queue.Write(&item);
            ASSERT_EQ(res, sme::QueueResult::kSuccessful);
        }

        for (auto& item : items) {
            ASSERT_TRUE(item.GetReferenceCount() != 0);
        }
    }

    pos = 0;
    for (auto& item : items) {
        ASSERT_TRUE(item.GetReferenceCount() == item_refs[pos++]);
    }
}

TEST(LockFreeQueueTest, TestThreadedWritingAndReading)
{
    // реализация в clang
    // alignas(16) std::atomic<__int128> il{};
    // assert(il.is_always_lock_free); //- не выдает
    // assert(il.is_lock_free()); // - выдает
    // alignas(16) std::atomic<Queue<int>::ItemLink> il;
    // assert(std::atomic<Queue<int>::ItemLink>::is_always_lock_free);

    constexpr size_t kItemCount = 350000;

    //constexpr size_t kWriterCount{1};
    //constexpr size_t kReaderCount{1};
    //constexpr size_t kWriterCount{4};
    //constexpr size_t kReaderCount{4};
    constexpr size_t kWriterCount{8};
    constexpr size_t kReaderCount{8};

    std::unique_ptr<Item<int>[]> data{new Item<int>[kItemCount]};

    std::atomic<size_t> write_pos{0};
    std::atomic<bool> writeAvailable{false};
    std::atomic<bool> readAvailable{false};

    sme::LockFreeQueue<Item<int>> queue;

    struct Writer {
        std::thread thread;
	    size_t write_count{0};
    };

    auto write_func = [&](Writer& writer) {
        while (!writeAvailable) {
            ;
        }

        size_t item_pos{0};
        while ((item_pos = write_pos++) < kItemCount) {
            auto res = queue.Write(&data[item_pos]);
	    	if (res != sme::QueueResult::kSuccessful) {
				//std::cerr << "Write failed, last write pos: " << write_pos << std::endl;
				break;
			}	
            ++writer.write_count;
        }
        //std::cout << "Write done" << std::endl;
    };

    struct Reader {
        std::thread thread;

        const size_t buffer_size{kItemCount + 1};
        std::unique_ptr<Item<int>*[]> buffer{new Item<int>*[buffer_size]};

        size_t read_count{0};
    };

    auto read_func = [&](Reader& reader) {
        while (!readAvailable) {
            ;
        }

        std::chrono::milliseconds timeout{0};	

        while (reader.read_count < reader.buffer_size) {
            auto [res, item] = queue.Read(timeout);
            if (item != nullptr) {
                reader.buffer[reader.read_count] = item;
                reader.read_count++;
            } else {
                //break;
                if (write_pos >= kItemCount) {
		    		//std::cout << "NR: " << reader.read_count << std::endl;
                    break;
				}    
            }
        }
    };

    for (auto i = 0U; i < kItemCount; i++)
        data[i].SetValue(i);

    std::array<Writer, kWriterCount> writers;
    std::array<Reader, kReaderCount> readers;

    for (auto& writer : writers)
        writer.thread = std::thread(write_func, std::ref(writer));
    for (auto& reader : readers)
        reader.thread = std::thread(read_func, std::ref(reader));

    writeAvailable = true;
    readAvailable = true;

    size_t total_write_count{0};

    for (auto& writer : writers) {
        writer.thread.join();
        total_write_count += writer.write_count;
    }
    std::cout << "WRITE total: " << total_write_count << std::endl;
    ASSERT_EQ(total_write_count, kItemCount);

    std::deque<Item<int>*> res_data;

    for (auto& reader : readers) {
        reader.thread.join();

        // std::cout << "-----------------------" << std::endl;
        for (auto i = 0U; i < reader.read_count; i++) {
            res_data.push_back(reader.buffer[i]);
            // std:: cout << reader.buffer[i] << " ";
        }
        // std::cout << std::endl;
        std::cout << "READ total:  " << reader.read_count << std::endl;
    }

    size_t add_count{0};
    std::pair<sme::QueueResult, Item<int>*> rd_res{};
    do {
        rd_res = queue.Read();
        if (rd_res.second != nullptr) {
            res_data.push_back(rd_res.second);
            add_count++;
        }
    } while (rd_res.first == sme::QueueResult::kSuccessful);

    std::cout << "Additional read count: " << add_count << std::endl;
    // ASSERT_EQ(add_count, 0);

    EXPECT_TRUE(queue.IsEmpty());
    EXPECT_EQ(queue.GetSize(), 0);

    std::sort(res_data.begin(), res_data.end(),
              [](Item<int>* lhs, Item<int>* rhs) { return lhs->GetValue() < rhs->GetValue(); });

    std::cout << std::dec << "RESULT, Read total: " << res_data.size() << std::endl;

    ASSERT_EQ(res_data.size(), kItemCount);

    for (auto i = 0u; i < res_data.size(); i++) {
        ASSERT_EQ(res_data[i]->GetValue(), data[i].GetValue())
            << "Invalid the item at the position " << i;
        ASSERT_EQ(res_data[i]->GetReferenceCount(), 1) << "Invalid the item reference count at the position " << i;
    }
}
