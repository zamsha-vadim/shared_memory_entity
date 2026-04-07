#include "sme/msg_channel.h"

#include <new>

//#include "sme/internal/queue.h"
#include "sme/internal/lf_queue.h"
#include "sme/mapped_obj.h"

#include <iostream>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,
// cppcoreguidelines-pro-type-reinterpret-cast)

namespace sme {

constexpr auto kControlValue{0x08122026UL};

struct SME_NO_EXPORT MessageChannelLayout {
    const size_t controlValue{kControlValue};

    MemorySpace memory_space;
    LockFreeQueue<Message> messages;
};

namespace {

auto CreateMessageChannelLayout(MemoryMap& mem_map) -> MessageChannelLayout*
{
    auto* map_addr = EnsureAddress<MessageChannelLayout>(mem_map, 0);

    auto* space_mem =
        static_cast<char*>(mem_map.GetAddress()) + sizeof(MessageChannelLayout);
    auto align_rest =
        reinterpret_cast<uintptr_t>(space_mem) % alignof(MessageChannelLayout);
    if (align_rest != 0)
        space_mem += alignof(MessageChannelLayout) - align_rest;

    auto space_size =
        mem_map.GetSize() - (space_mem - static_cast<const char*>(mem_map.GetAddress()));

    return new (map_addr) MessageChannelLayout{
        .memory_space = MemorySpace{space_mem, space_size,
                                    mem_map.IsShared() ? Synchronizer::Type::kShared
                                                       : Synchronizer::Type::kPrivate},
        .messages = {}};
}

auto GetMessageChannelLayout(MemoryMap& mem_map) -> MessageChannelLayout&
{
    auto& obj = GetObject<MessageChannelLayout>(mem_map, 0);
    if (obj.controlValue != kControlValue)
        throw std::invalid_argument("Invalid message channel");
    return obj;
}

auto SupplyMessageChannelLayout(MemoryMap& mem_map,
                                MessageChannel::InitialState init_state)
    -> MessageChannelLayout&
{
    return (init_state == MessageChannel::InitialState::kCreate)
               ? *CreateMessageChannelLayout(mem_map)
               : GetMessageChannelLayout(mem_map);
}

auto CreateMessage(MemoryDomain& mem_domain) -> IntrusivePtr<Message>
{
    auto* msg = sme::Create<Message>(mem_domain, mem_domain, MessageDeleter{});
    if (msg == nullptr)
        throw std::bad_alloc{};

    assert((reinterpret_cast<uintptr_t>(msg) % alignof(sme::ItemLink)) == 0);

    return IntrusivePtr<Message>{msg};
}

}  // namespace

// class Message

Message::Message(MemoryDomain& mem_domain, MessageDeleter deleter) noexcept
    : mem_domain_{&mem_domain}, deleter_{std::move(deleter)}
{
}

Message::~Message() {
    assert(ref_counter_.GetValue() == 0);
}

auto Message::GetAllocationDomain() -> MemoryDomain&
{
    if (!IsValid())
        throw std::logic_error("Message is not valid");

    return *mem_domain_;
}

auto Message::GetAllocationDomain() const -> const MemoryDomain&
{
    if (!IsValid())
        throw std::logic_error("Out message is not valid");

    return *mem_domain_;
}

auto Message::GetRootObjectDefintion() const -> const MessageRootObjectDefinition&
{
    ValidateState();

    return root_def_;
}

void Message::SetRootObjectDefinition(MessageRootObjectDefinition root_def)
{
    ValidateState();

    if (root_def.object == nullptr)
        throw std::invalid_argument("Message root object is null");

    root_def_ = std::move(root_def);
}

void Message::SetRootObject(void* obj, size_t type)
{
    SetRootObjectDefinition({.object = obj, .type = type});
}

auto Message::IsValid() const noexcept -> bool
{
    return mem_domain_ != nullptr;
}

void Message::ValidateState() const
{
    if (!IsValid())
        throw std::logic_error("Message is invalid");
}

auto Message::AddReference(ReferenceCounterType amount) const noexcept
    -> ReferenceCounterType
{
    return ref_counter_.Increment(amount);
}

auto Message::RemoveReference() const noexcept -> ReferenceCounterType
{
    auto counter = ref_counter_.Decrement();
    if (counter == 0) {
        auto deleter = std::move(deleter_);
        deleter(const_cast<Message*>(this));
    }

    return counter;
}

auto Message::GetReferenceCount() const noexcept -> ReferenceCounterType
{
    return ref_counter_.GetValue();
}

auto Message::GetItemLink() const noexcept -> const std::atomic<ItemLink>&
{
    return item_link_;
}

auto Message::GetItemLink() noexcept -> std::atomic<ItemLink>&
{
    return item_link_;
}

// class MessageDeleter

void MessageDeleter::operator()(Message* obj) noexcept
{
    if (obj == nullptr)
        return;

    auto& mem_domain = obj->GetAllocationDomain();

    sme::Delete(mem_domain, Pointer<Message>{obj});
    sme::DeleteMemoryDomain(&mem_domain);
}

// class MessageWriter

MessageWriter::MessageWriter(MessageChannelLayout& data_layout) noexcept
    : data_layout_{data_layout}
{
}

MessageWriter::~MessageWriter() {}

auto MessageWriter::CreateMessage() -> IntrusivePtr<Message>
{
    auto mem_domain = CreateMemoryDomain(data_layout_.memory_space);
    if (mem_domain == nullptr)
        throw std::bad_alloc{};

    try {
        return sme::CreateMessage(*mem_domain);
    } catch (...) {
        sme::DeleteMemoryDomain(mem_domain);
        throw;
    }
}

auto MessageWriter::Commit(IntrusivePtr<Message>& msg_ptr) -> QueueResult
{
    if (!msg_ptr)
        throw std::invalid_argument("Output message is null");

    if (msg_ptr->GetRootObjectDefintion().object == nullptr)
        throw std::invalid_argument("Output message root object not defined");

    auto& msg_domain = msg_ptr->GetAllocationDomain();
    msg_domain.DisableAllocationExtensible();

    Message* msg = msg_ptr.Detach();

    QueueResult res{};

    try {
        res = data_layout_.messages.Write(msg);

        if (res != QueueResult::kSuccessful)
            msg_ptr.Adopt(msg);
    } catch (...) {
        msg_ptr.Adopt(msg);

        throw;
    }

    return res;    
}

// class MessageReader

MessageReader::MessageReader(MessageChannelLayout& data_layout) noexcept
    : data_layout_{data_layout}
{
}

MessageReader::~MessageReader() {}

auto MessageReader::Read() -> std::pair<QueueResult, IntrusivePtr<Message>>
{
    auto [res, msg] = data_layout_.messages.Read();
    return {res, IntrusivePtr<Message>{msg, ReferenceAssign::kAdopt}};
}

auto MessageReader::Read(const std::chrono::milliseconds& timeout)
    -> std::pair<QueueResult, IntrusivePtr<Message>>
{
    auto [res, msg] = data_layout_.messages.Read(timeout);
    return {res, IntrusivePtr<Message>{msg, ReferenceAssign::kAdopt}};
}

auto MessageReader::Wait(const std::chrono::milliseconds& timeout) const -> QueueResult
{
    return data_layout_.messages.WaitForReading(timeout);
}

auto MessageReader::IsEmpty() const noexcept -> bool
{
    return !data_layout_.messages.IsEmpty();
}

// class MessageChannel

MessageChannel::MessageChannel(MemoryMap mem_map, InitialState init_state)
    : mem_map_{std::move(mem_map)},
      data_layout_{SupplyMessageChannelLayout(mem_map_, init_state)},
      reader_{data_layout_}, writer_{data_layout_}
{
}

auto MessageChannel::GetReader() -> MessageReader&
{
    return reader_;
}

auto MessageChannel::GetWriter() -> MessageWriter&
{
    return writer_;
}

void MessageChannel::Disable(bool state) {
    data_layout_.messages.Disable(state);
}

auto MessageChannel::IsDisabled() const noexcept -> bool
{
    return data_layout_.messages.IsDisabled();
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,
// cppcoreguidelines-pro-type-reinterpret-cast)
