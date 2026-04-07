#ifndef SME_QUEUE_TCC
#define SME_QUEUE_TCC

#include <cassert>

namespace sme {

template <typename T>
void Queue<T>::Write(ItemPointer new_item) noexcept
{
    assert(new_item != nullptr);

    new_item->SetNext(nullptr);

    if (last_ != nullptr) {
        last_->SetNext(new_item);
        last_ = new_item;
    } else {
        first_ = new_item;
        last_ = new_item;
    }
}

template <typename T>
auto Queue<T>::Read() noexcept -> ItemPointer
{
    if (first_ == nullptr)
        return nullptr;

    auto item = first_;

    first_ = first_->GetNext();
    if (first_ == nullptr)
        last_ = nullptr;

    item->SetNext(nullptr);

    return item;
}

template <typename T>
auto Queue<T>::IsEmpty() const noexcept -> bool
{
    return (first_ == nullptr && last_ == nullptr);
}

}  // namespace sme

#endif  // SME_QUEUE_TCC
