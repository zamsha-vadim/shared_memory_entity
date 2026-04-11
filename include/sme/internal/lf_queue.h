#ifndef SME_INTERNAL_LF_QUEUE_H
#define SME_INTERNAL_LF_QUEUE_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <new>
#include <utility>

#include "sme/internal/item_link.h"
#include "sme/internal/queue_res.h"
#include "sme/sme_export.h"

#if defined(__x86_64__)
#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#error \
    " the target processor doen't support atomic compare and swap operations on operands 16 bytes in length"
#endif
#endif

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t kCacheLineSize = std::hardware_constructive_interference_size;
#else
constexpr size_t kCacheLineSize = 64;
#endif

namespace sme {

template <typename ItemT>
class alignas(kCacheLineSize) LockFreeQueue final {
    static_assert(alignof(ItemT) >= kDWCASAlign,
                  "The item type alignment must be equal or greater 16");

    static_assert(HasAddReference<ItemT>::value,
                  "The item class must have the method \"auto AddReference(CounterType "
                  "count) const noexcept -> CounterType\"");

    static_assert(HasRemoveReference<ItemT>::value,
                  "The item class must have the method \"auto RemoveReference() const "
                  "noexcept -> CounterType\"");

    static_assert(HasItemLink<ItemT>::value,
                  "The item class must have the method \"auto GetItemLink() const "
                  "noexcept -> std::atomic<sme::ItemLink>&\"");

#if defined(__clang__)
    static_assert(std::atomic<ItemLink>::is_always_lock_free,
                  "128-bit atomics are not lock-free");
#endif

   public:
    using ItemType = ItemT;

   public:
    LockFreeQueue() = default;
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    auto operator=(const LockFreeQueue&) -> LockFreeQueue& = delete;
    auto operator=(LockFreeQueue&&) noexcept -> LockFreeQueue& = delete;
    ~LockFreeQueue();

    [[nodiscard]] auto Write(ItemType* new_item) noexcept -> QueueResult;

    [[nodiscard]] auto Read() noexcept -> std::pair<QueueResult, ItemType*>;
    [[nodiscard]] auto Read(const std::chrono::milliseconds& timeout)
        -> std::pair<QueueResult, ItemType*>;
    [[nodiscard]] auto WaitForReading(
        const std::chrono::milliseconds& timeout = std::chrono::milliseconds::max()) const
        -> QueueResult;

    [[nodiscard]] auto GetSize() const noexcept -> size_t;
    [[nodiscard]] auto IsEmpty() const noexcept -> bool;

    void Disable(bool disabled = true);
    [[nodiscard]] auto IsDisabled() const noexcept -> bool;

   private:
    [[nodiscard]] auto WriteItem(ItemType* new_item) noexcept -> QueueResult;
    [[nodiscard]] auto ReadItem() noexcept -> ItemType*;
    [[nodiscard]] auto WaitForItemReading(const std::chrono::milliseconds& timeout) const
        -> QueueResult;

    void ReleaseItem(ItemType*);

    void IncrementMessageCountMarker() noexcept;
    void DecrementMessageCountMarker() noexcept;

    void NotifyReaders() noexcept;

    void Clear() noexcept;

    auto GetObjectOffset(const ItemType* obj) const noexcept -> ObjectOffset;
    auto GetObjectAddress(ObjectOffset ofs_ptr) const noexcept -> ItemType*;

   private:
    static constexpr uint32_t kDisabledState{0x80000000};
    static constexpr uint32_t kUnmarkedState{0x0000FFFF};

    alignas(kCacheLineSize) std::atomic<ItemLink> read_link_{};
    alignas(kCacheLineSize) std::atomic<ObjectOffset> last_item_ofp_{0};

    alignas(kCacheLineSize) mutable std::atomic<uint32_t> act_state_{kUnmarkedState};
    mutable std::atomic<uint32_t> wait_count_{0};

    std::atomic<uint32_t> size_{0};
};

}  // namespace sme

#include "sme/internal/lf_queue.tcc"

#endif  // SME_INTERNAL_LF_QUEUE_H_
