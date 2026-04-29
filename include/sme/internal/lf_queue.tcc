#ifndef SME_INTERNAL_LF_QUEUE_TCC
#define SME_INTERNAL_LF_QUEUE_TCC

#include <cassert>
#include <limits>

#include "sme/futex.h"

namespace sme {

template <typename ItemT>
LockFreeQueue<ItemT>::~LockFreeQueue()
{
    Clear();
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::Write(ItemType* new_item) noexcept -> QueueResult
{
    if (IsDisabled())
        return QueueResult::kRejected;

    auto res = WriteItem(new_item);
    if (res == QueueResult::kSuccessful) {
        IncrementMessageCountMarker();
        NotifyReaders();
    }

    return res;
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::WriteItem(ItemType* new_item) noexcept -> QueueResult
{
    assert(new_item != nullptr);

    constexpr uint16_t kItemRefCount{3};
    [[maybe_unused]] uint16_t obj_counter{};

    obj_counter = new_item->AddReference(kItemRefCount);
    assert(obj_counter == (kItemRefCount + 1));

    auto new_item_ofp = GetObjectOffset(new_item);

    ItemLink link{.basic = new_item_ofp, .next = 0};
    new_item->GetItemDescriptor().link.store(link, std::memory_order_relaxed);

    auto prev_item_ofp = last_item_ofp_.load(std::memory_order_relaxed);

    while (!last_item_ofp_.compare_exchange_weak(prev_item_ofp, new_item_ofp,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
        ;
    }

    ItemType* prev_item = GetObjectAddress(prev_item_ofp);
    if (prev_item != nullptr) {
        auto& item_descr = prev_item->GetItemDescriptor();

        auto link = item_descr.link.load(std::memory_order_acquire);
        link.next = new_item_ofp;

        item_descr.link.store(link, std::memory_order_release);

        obj_counter = prev_item->RemoveReference();
        assert(obj_counter > 0);
    }

    auto curr_read_link = read_link_.load(std::memory_order_acquire);

    if (curr_read_link.next == 0) {
        const auto* basic_item = GetObjectAddress(curr_read_link.basic);

        if (basic_item == prev_item || prev_item == nullptr) {
            for (;;) {
                auto updated_read_link = curr_read_link;
                updated_read_link.next = GetObjectOffset(new_item);

                bool completed = read_link_.compare_exchange_strong(
                    curr_read_link, updated_read_link, /*std::memory_order_release*/ std::memory_order_relaxed);

                if (completed || (ExtractOffset(curr_read_link.basic) !=
                                  ExtractOffset(updated_read_link.basic))) {
                    break;
                }
            }
        }
    }

    obj_counter = new_item->RemoveReference();
    assert(obj_counter > 0);

    size_.fetch_add(1, std::memory_order_release);

    return QueueResult::kSuccessful;
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::Read() noexcept -> std::pair<QueueResult, ItemType*>
{
    if (IsDisabled())
        return {QueueResult::kRejected, nullptr};

    auto* item = ReadItem();
    if (item == nullptr)
        return {QueueResult::kFailed, nullptr};

    DecrementMessageCountMarker();

    return std::make_pair(QueueResult::kSuccessful, item);
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::Read(const std::chrono::milliseconds& timeout)
    -> std::pair<QueueResult, ItemType*>
{
    if (timeout == std::chrono::milliseconds::min())
        return Read();

    auto curr_timeout{timeout};

    auto begin_time = std::chrono::system_clock::now();

    while (!IsDisabled()) {
        auto* item = ReadItem();
        if (item != nullptr) {
            DecrementMessageCountMarker();

            return std::make_pair(QueueResult::kSuccessful, item);
        }

        auto wait_res = WaitForItemReading(curr_timeout);
        if (wait_res != QueueResult::kSuccessful)
            return {wait_res, nullptr};

        auto elapsed_time = std::chrono::system_clock::now() - begin_time;
        if (elapsed_time >= timeout)
            return {QueueResult::kTimeout, nullptr};
        curr_timeout =
            timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time);
    }

    return {QueueResult::kRejected, nullptr};
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::ReadItem() noexcept -> ItemType*
{
    ItemLink holder_link{};
    ItemLink next_link{};
    
    auto curr_link = read_link_.load(std::memory_order_acquire);

    for (;;) {
        if (!IncrementUseCounter(curr_link, holder_link))
            return nullptr;

        curr_link = holder_link;

        auto* curr_item = GetObjectAddress(curr_link.next);
        auto& curr_item_descriptor = curr_item->GetItemDescriptor();
        next_link = curr_item_descriptor.link.load(std::memory_order_acquire);

        bool completed{};

        for(;;) {
            completed = read_link_.compare_exchange_strong(curr_link, next_link,
                                                           /*std::memory_order_acq_rel*/ std::memory_order_relaxed);
            if (completed) {
                auto read_counter = ExtractUseCounter(curr_link.basic);
                curr_item_descriptor.use_counter.fetch_add(read_counter,
                                                           std::memory_order_relaxed);
                break;
            } else {
                if (ExtractOffset(curr_link.basic) != ExtractOffset(holder_link.basic)) {
                    auto prev_use_counter = curr_item_descriptor.use_counter.fetch_sub(
                        1, std::memory_order_relaxed);
                    if (prev_use_counter == 1)
                        curr_item->RemoveReference();

                    break;
                }
            }
        }

        if (completed)
            break;
    }

    assert(curr_link.next != 0);

    auto* res_item = GetObjectAddress(curr_link.next);
    if (res_item == nullptr)
        return nullptr;

    size_.fetch_sub(1, std::memory_order_relaxed);

    auto& item_descriptor = res_item->GetItemDescriptor();
    auto item_link = item_descriptor.link.load(std::memory_order_acquire);

    if (/*next_link.basic == item_link.basic &&*/ next_link.next == 0 &&
        item_link.next != 0) {
        read_link_.compare_exchange_strong(next_link, item_link,
                                           std::memory_order_relaxed);
    }

    auto prev_use_counter =
        item_descriptor.use_counter.fetch_sub(1, std::memory_order_relaxed);
    if (prev_use_counter == 1) {
        [[maybe_unused]] auto obj_counter = res_item->RemoveReference();

        assert(obj_counter > 0);
        assert(obj_counter != std::numeric_limits<decltype(obj_counter)>::max());
    }

    auto last_item_ofp = last_item_ofp_.load(std::memory_order_acquire);
    const auto* last_item = GetObjectAddress(last_item_ofp);

    if (res_item == last_item) {
        if (last_item_ofp_.compare_exchange_strong(last_item_ofp, 0,
                                                   std::memory_order_relaxed)) {
            [[maybe_unused]] auto obj_counter = res_item->RemoveReference();

            assert(obj_counter >= 1);
            assert(obj_counter != std::numeric_limits<decltype(obj_counter)>::max());
        }
    }

    return res_item;
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::IncrementUseCounter(ItemLink& curr_link,
                                               ItemLink& holder_link) noexcept -> bool
{
    if (curr_link.next == 0)
        return false;

    holder_link = curr_link;

    IncreaseUseCounter(holder_link.basic, 1);

    while (!read_link_.compare_exchange_weak(curr_link, holder_link,
                                               /*std::memory_order_acq_rel*/ std::memory_order_relaxed) &&
           curr_link.next != 0) {
        holder_link = curr_link;
        IncreaseUseCounter(holder_link.basic, 1);
    }

    return (curr_link.next != 0);
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::WaitForReading(const std::chrono::milliseconds& timeout) const
    -> QueueResult
{
    if (IsDisabled())
        return QueueResult::kRejected;
    return WaitForItemReading(timeout);
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::WaitForItemReading(
    const std::chrono::milliseconds& timeout) const -> QueueResult
{
    if (timeout == std::chrono::milliseconds::min())
        return QueueResult::kSuccessful;

    wait_count_.fetch_add(1, std::memory_order_release);

    try {
        auto res = FutexWait(act_state_, kUnmarkedState, timeout);

        wait_count_.fetch_sub(1, std::memory_order_release);

        if (res == FutexResult::kTimeout)
            return QueueResult::kTimeout;
        return (!IsDisabled() ? QueueResult::kSuccessful : QueueResult::kRejected);

    } catch (const std::exception& ex) {
        wait_count_.fetch_sub(1, std::memory_order_release);

        throw std::runtime_error(std::string("Lock free queue wait error: ") + ex.what());
    }
}

template <typename ItemT>
void LockFreeQueue<ItemT>::NotifyReaders() noexcept
{
    if (wait_count_.load(std::memory_order_acquire) == 0)
        return;

    try {
        constexpr uint32_t kOneItemCount{1};

        FutexWake(act_state_, kOneItemCount);

    } catch (const std::exception& ex) {
        // assert(1 == 0);
        // throw std::runtime_error(std::string("Lock free queue wake error: ") +
        // ex.what());
    }
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::GetSize() const noexcept -> size_t
{
    return size_.load(std::memory_order_acquire);
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::IsEmpty() const noexcept -> bool
{
    return read_link_.load(std::memory_order_acquire).next == 0;
}

template <typename ItemT>
void LockFreeQueue<ItemT>::Disable(bool disabled)
{
    if (disabled == true) {
        act_state_.fetch_or(kDisabledState, std::memory_order_release);

        NotifyReaders();
    } else {
        act_state_.fetch_and(~kDisabledState, std::memory_order_release);
    }
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::IsDisabled() const noexcept -> bool
{
    return (act_state_.load(std::memory_order_acquire) & kDisabledState) != 0;
}

template <typename ItemT>
void LockFreeQueue<ItemT>::Clear() noexcept
{
    auto last_item_ofp = last_item_ofp_.load(std::memory_order_acquire);

    auto begin_link = read_link_.load(std::memory_order_acquire);
    auto item_ofp = begin_link.next;

    while (item_ofp != 0) {
        auto* item = GetObjectAddress(item_ofp);
        if (item == nullptr)
            break;

        auto item_link = item->GetItemDescriptor().link.load(std::memory_order_acquire);
        auto next_item_ofp = item_link.next;

        item_link.next = 0;
        item->GetItemDescriptor().link.store(item_link, std::memory_order_release);

        auto obj_counter = item->RemoveReference();
        if (obj_counter > 0 && item_ofp == last_item_ofp)
            item->RemoveReference();

        item_ofp = next_item_ofp;
    }

    read_link_.store(ItemLink{}, std::memory_order_release);
    last_item_ofp_.store(0, std::memory_order_release);
}

template <typename ItemT>
void LockFreeQueue<ItemT>::IncrementMessageCountMarker() noexcept
{
//    assert(act_state_.load(std::memory_order_acquire) < std::numeric_limits<uint32_t>::max());

    act_state_.fetch_add(1, std::memory_order_relaxed);
}

template <typename ItemT>
void LockFreeQueue<ItemT>::DecrementMessageCountMarker() noexcept
{
//    assert(act_state_.load(std::memory_order_acquire) > 0);

    act_state_.fetch_sub(1, std::memory_order_relaxed);
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::GetObjectOffset(const ItemType* obj) const noexcept
    -> ObjectOffset
{
    return sme::GetObjectOffset<LockFreeQueue, ItemType>(this, obj);
}

template <typename ItemT>
auto LockFreeQueue<ItemT>::GetObjectAddress(ObjectOffset ofs_ptr) const noexcept
    -> ItemType*
{
    return sme::GetObjectAddress<LockFreeQueue, ItemType>(this, ofs_ptr);
}

}  // namespace sme

#endif  // SME_INTERNAL_LF_QUEUE_TCC
