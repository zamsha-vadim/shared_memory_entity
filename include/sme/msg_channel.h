#ifndef SME_MSG_CHANNEL_H
#define SME_MSG_CHANNEL_H

#include <chrono>
#include <cstring>
#include <utility>

#include "sme/alloc_util.h"
#include "sme/internal/item_link.h"
#include "sme/internal/queue_res.h"
#include "sme/mem_domain.h"
#include "sme/mem_map.h"
#include "sme/sme_export.h"
#include "sme/sync_type.h"

namespace sme {

struct MessageChannelLayout;

struct SME_EXPORT MessageRootObjectDefinition final {
    Pointer<void> object;
    size_t type{};
};

template <typename T>
class Queue;

class Message;

class SME_EXPORT MessageDeleter final {
   public:
    void operator()(Message* obj) noexcept;
};

class SME_EXPORT __attribute__((aligned(16))) Message {
    friend class Queue<Message>;

   public:
    using ReferenceCounterType = ReferenceCounter<>::ValueType;

   public:
    Message(MemoryDomain& mem_domain, MessageDeleter deleter = MessageDeleter{}) noexcept;

    Message(const Message&) = delete;
    Message(Message&&) = delete;
    auto operator=(const Message&) -> Message& = delete;
    auto operator=(Message&&) -> Message& = delete;

    ~Message();

    auto GetAllocationDomain() -> MemoryDomain&;
    auto GetAllocationDomain() const -> const MemoryDomain&;

    auto GetRootObjectDefintion() const -> const MessageRootObjectDefinition&;
    void SetRootObjectDefinition(MessageRootObjectDefinition root_def);
    void SetRootObject(void* obj, size_t type_id);

    auto IsValid() const noexcept -> bool;

    auto AddReference(ReferenceCounterType amount = 1) const noexcept -> ReferenceCounterType;
    auto RemoveReference() const noexcept -> ReferenceCounterType;
    auto GetReferenceCount() const noexcept -> ReferenceCounterType;

    auto GetItemLink() const noexcept -> const std::atomic<ItemLink>&;
    auto GetItemLink() noexcept -> std::atomic<ItemLink>&;

   private:
    void ValidateState() const;

   private:
    mutable std::atomic<ItemLink> item_link_;
    mutable ReferenceCounter<> ref_counter_;

    Pointer<MemoryDomain> mem_domain_;
    MessageRootObjectDefinition root_def_{};

    mutable MessageDeleter deleter_;
};

class SME_EXPORT MessageWriter final {
   public:
    MessageWriter(MessageChannelLayout& data_layout) noexcept;

    MessageWriter(const MessageWriter&) = delete;
    MessageWriter(MessageWriter&&) = delete;
    auto operator=(const MessageWriter&) -> MessageWriter& = delete;
    auto operator=(MessageWriter&&) -> MessageWriter& = delete;

    ~MessageWriter();

    [[nodiscard]] auto CreateMessage() -> IntrusivePtr<Message>;
    [[nodiscard]] auto Commit(IntrusivePtr<Message>& msg) -> QueueResult;

    void Disable(bool state = true);
    [[nodiscard]] auto IsDisabled() const noexcept -> bool;

   private:
    void Close() noexcept;

   private:
    MessageChannelLayout& data_layout_;
};

class SME_EXPORT MessageReader final {
   public:
    MessageReader(MessageChannelLayout& data_layout) noexcept;

    MessageReader(const MessageReader&) = delete;
    MessageReader(MessageReader&&) = delete;
    auto operator=(const MessageReader&) -> MessageReader& = delete;
    auto operator=(MessageReader&&) -> MessageReader& = delete;

    ~MessageReader();

    [[nodiscard]] auto Read() -> std::pair<QueueResult, IntrusivePtr<Message>>;
    [[nodiscard]] auto Read(const std::chrono::milliseconds& timeout)
        -> std::pair<QueueResult, IntrusivePtr<Message>>;
    [[nodiscard]] auto Wait(const std::chrono::milliseconds& timeout =
                                std::chrono::milliseconds::max()) const -> QueueResult;

    [[nodiscard]] auto IsEmpty() const noexcept -> bool;

   private:
    MessageChannelLayout& data_layout_;
};

class SME_EXPORT MessageChannel final {
   public:
    enum InitialState { kCreate, kOpen };

   public:
    MessageChannel(MemoryMap mem_map, InitialState init_state);

    MessageChannel(const MessageChannel&) = delete;
    MessageChannel(MessageChannel&&) = delete;
    auto operator=(const MessageChannel&) -> MessageChannel& = delete;
    auto operator=(MessageChannel&&) -> MessageChannel& = delete;

    ~MessageChannel() = default;

    auto GetReader() -> MessageReader&;
    auto GetWriter() -> MessageWriter&;

    void Disable(bool state = true);
    [[nodiscard]] auto IsDisabled() const noexcept -> bool;

   private:
    MemoryMap mem_map_;
    MessageChannelLayout& data_layout_;

    MessageReader reader_;
    MessageWriter writer_;
};

}  // namespace sme

#endif  // SME_MSG_CHANNEL_H
