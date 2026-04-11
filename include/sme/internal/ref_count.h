#ifndef SME_INTERNAL_REF_COUNT_H
#define SME_INTERNAL_REF_COUNT_H

#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "sme/sme_export.h"

namespace sme {

template <typename T = uint16_t>
class SME_EXPORT ReferenceCounter {
    static_assert(std::is_unsigned_v<T>, "Counter type must be unsigned");

   public:
    using ValueType = T;

   public:
    explicit ReferenceCounter(ValueType value = 0) : counter_{value} {}

    auto Increment(ValueType amount = 1) const noexcept -> ValueType;
    auto Decrement() const noexcept -> ValueType;
    auto GetValue() const noexcept -> ValueType;

   private:
    mutable std::atomic<ValueType> counter_;
};

template <typename T>
auto ReferenceCounter<T>::Increment(ValueType amount) const noexcept -> ValueType
{
    assert(amount > 0);

    auto prev_value = counter_.fetch_add(amount, std::memory_order_acquire);

    auto updated_value = prev_value + amount;
    assert(updated_value > prev_value);

    return updated_value;
}

template <typename T>
auto ReferenceCounter<T>::Decrement() const noexcept -> ValueType
{
    auto prev_value = counter_.fetch_sub(1, std::memory_order_release);

    auto updated_value = prev_value - 1;
    assert(updated_value != std::numeric_limits<ValueType>::max());

    return updated_value;
}

template <typename T>
auto ReferenceCounter<T>::GetValue() const noexcept -> ValueType
{
    return counter_.load(std::memory_order_acquire);
}

template <typename, typename = std::void_t<>>
struct HasAddReference : std::false_type {};

template <typename T>
struct HasAddReference<T, std::void_t<decltype(std::declval<T>().AddReference(1))>>
    : std::true_type {};

template <typename, typename = std::void_t<>>
struct HasRemoveReference : std::false_type {};

template <typename T>
struct HasRemoveReference<T, std::void_t<decltype(std::declval<T>().RemoveReference())>>
    : std::true_type {};

enum class SME_EXPORT ReferenceAssign { kAdd, kAdopt };

template <typename T>
class SME_EXPORT IntrusivePtr final {
    static_assert(
        HasAddReference<T>::value,
        "The template class must have the method \"auto AddReference(CounterType "
        "count) const noexcept -> CounterType\"");

    static_assert(
        HasRemoveReference<T>::value,
        "The itemplate class must have the method \"auto RemoveReference() const "
        "noexcept -> CounterType\"");

   public:
    enum class AssignType { kAddReference, kAttach };

   public:
    IntrusivePtr() noexcept = default;
    explicit IntrusivePtr(T* obj) noexcept;
    IntrusivePtr(T* obj, ReferenceAssign assign_type) noexcept;
    explicit IntrusivePtr(std::nullptr_t) noexcept : IntrusivePtr{} {}

    IntrusivePtr(const IntrusivePtr& src) noexcept { Copy(src); }
    IntrusivePtr(IntrusivePtr&& src) noexcept { Move(src); }

    auto operator=(const IntrusivePtr& src) noexcept -> IntrusivePtr&;
    auto operator=(IntrusivePtr&& src) noexcept -> IntrusivePtr&;

    ~IntrusivePtr() { Release(); }

    auto IsValid() const noexcept -> bool { return obj_ != nullptr; }
    explicit operator bool() const noexcept { return IsValid(); }

    void Release() noexcept;

    void Adopt(T* obj) noexcept;
    [[nodiscard]] auto Detach() noexcept -> T*;

    [[nodiscard]] auto operator->() const noexcept -> T*
    {
        assert(IsValid());
        return obj_;
    }

    [[nodiscard]] auto operator*() const noexcept -> T&
    {
        assert(IsValid());
        return *obj_;
    }

    [[nodiscard]] auto Get() const noexcept -> T* { return obj_; }

   private:
    auto IsSame(const IntrusivePtr& other) const noexcept -> bool;

    void Copy(const IntrusivePtr& src) noexcept;
    void Move(IntrusivePtr& src) noexcept;

   private:
    T* obj_{nullptr};
};

template <typename T>
IntrusivePtr<T>::IntrusivePtr(T* obj) noexcept : obj_{obj}
{
    if (obj_ != nullptr)
        obj_->AddReference();
}

template <typename T>
IntrusivePtr<T>::IntrusivePtr(T* obj, ReferenceAssign assign_type) noexcept : obj_{obj}
{
    if (obj_ != nullptr && assign_type == ReferenceAssign::kAdd)
        obj_->AddReference();
}

// NOLINTBEGIN(bugprone-unhandled-self-assignment)

template <typename T>
auto IntrusivePtr<T>::operator=(const IntrusivePtr& src) noexcept -> IntrusivePtr&
{
    Copy(src);
    return *this;
}

// NOLINTEND(bugprone-unhandled-self-assignment)

template <typename T>
auto IntrusivePtr<T>::operator=(IntrusivePtr&& src) noexcept -> IntrusivePtr&
{
    Move(src);
    return *this;
}

template <typename T>
void IntrusivePtr<T>::Copy(const IntrusivePtr& src) noexcept
{
    if (IsSame(src))
        return;

    Release();

    obj_ = src.obj_;
    if (obj_ != nullptr)
        obj_->AddReference();
}

template <typename T>
void IntrusivePtr<T>::Move(IntrusivePtr& src) noexcept
{
    if (IsSame(src))
        return;

    Release();

    obj_ = src.obj_;
    src.obj_ = nullptr;
}

template <typename T>
auto IntrusivePtr<T>::IsSame(const IntrusivePtr& other) const noexcept -> bool
{
    return (&other == this || other.obj_ == obj_);
}

template <typename T>
void IntrusivePtr<T>::Release() noexcept
{
    if (obj_ != nullptr) {
        obj_->RemoveReference();
        obj_ = nullptr;
    }
}

template <typename T>
void IntrusivePtr<T>::Adopt(T* obj) noexcept
{
    Release();

    obj_ = obj;
}

template <typename T>
auto IntrusivePtr<T>::Detach() noexcept -> T*
{
    auto* obj = obj_;
    obj_ = nullptr;
    return obj;
}

}  // namespace sme

#endif  // SME_INTERNAL_REF_COUNT_H
