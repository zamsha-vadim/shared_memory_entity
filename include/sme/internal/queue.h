#ifndef SME_QUEUE_H
#define SME_QUEUE_H

#include <utility>
#include <type_traits>

#include <sme/mem_ptr.h>

namespace sme {

template <typename, typename = std::void_t<>>
struct HasGetNext : std::false_type {};

template <typename T>
struct HasGetNext<T, std::void_t<decltype(std::declval<T>().GetNext())>>
    : std::true_type {};

template <typename, typename = std::void_t<>>
struct HasSetNext : std::false_type {};

template <typename T>
struct HasSetNext<T, std::void_t<decltype(std::declval<T>().SetNext(sme::Pointer<T>{}))>>
    : std::true_type {};

template <typename T>
class Queue final {
   public:
    using ItemType = T;
    using ItemPointer = Pointer<T>;

    static_assert(
        HasGetNext<T>::value,
        "ItemType class must have method like as \"auto ItemType::GetNext() const "
        "noexcept -> Pointer<ItemType>\"");

    static_assert(HasSetNext<T>::value,
                  "ItemType class must have method like as \"void "
                  "SetNext(Pointer<ItemType>) noexcept\"");

   public:
    Queue() = default;
    Queue(const Queue&) = delete;
    Queue(Queue&&) = default;
    auto operator=(const Queue&) -> Queue& = delete;
    auto operator=(Queue&&) -> Queue& = default;
    ~Queue() = default;

    void Write(ItemPointer obj) noexcept;
    [[nodiscard]] auto Read() noexcept -> ItemPointer;

    auto IsEmpty() const noexcept -> bool;

   private:
    ItemPointer first_;
    ItemPointer last_;
};

}  // namespace sme

#include "sme/internal/queue.tcc"

#endif  // SME_QUEUE_H
