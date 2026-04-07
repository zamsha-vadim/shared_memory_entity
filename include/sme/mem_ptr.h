#ifndef SME_MEM_PTR_H
#define SME_MEM_PTR_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <new>
#include <stdexcept>
#include <type_traits>

#include "sme/sme_export.h"

namespace sme {

// NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast,
// cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-pointer-arithmetic)

template <typename T>
class SME_EXPORT Pointer final {
   public:
    using value_type = T;
    using const_value_type = std::add_const_t<T>;

    using pointer = value_type*;
    using const_pointer = std::add_const_t<pointer>;

    using reference = std::add_lvalue_reference_t<value_type>;
    using const_reference = std::add_const_t<reference>;

    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    template <typename U>
    using rebind = Pointer<U>;

    using adapted_value_type =
        std::conditional_t<!std::is_void_v<value_type>, value_type, char*>;

    template <typename U>
    using if_other_type_t =
        typename std::enable_if_t<!std::is_same_v<value_type, U>, bool>;

   public:
    Pointer() noexcept;

    Pointer(pointer addr) noexcept;
    Pointer(std::nullptr_t) noexcept;

    Pointer(const Pointer& ptr) noexcept;
    Pointer(Pointer&& ptr) noexcept;

    template <typename U, if_other_type_t<U> = true>
    Pointer(const Pointer<U>& ptr) noexcept
    {
        auto* addr = static_cast<pointer>(ptr.GetAddress());
        SetAddress(addr);
    }

    template <typename U, if_other_type_t<U> = true>
    explicit Pointer(Pointer<U>&& ptr) noexcept
    {
        auto* addr = static_cast<pointer>(ptr.GetAddress());
        SetAddress(addr);

        ptr.Reset();
    }

    ~Pointer() = default;

    auto operator=(const Pointer& ptr) noexcept -> Pointer&;
    auto operator=(Pointer&& ptr) noexcept -> Pointer&;
    auto operator=(std::nullptr_t) noexcept -> Pointer&;

    template <typename U, if_other_type_t<U> = true>
    auto operator=(const Pointer<U>& ptr) noexcept -> Pointer&
    {
        auto* addr = static_cast<pointer>(ptr.GetAddress());
        SetAddress(addr);

        return *this;
    }

    template <typename U, if_other_type_t<U> = true>
    auto operator=(Pointer<U>&& ptr) noexcept -> Pointer&
    {
        auto* addr = static_cast<pointer>(ptr.GetAddress());
        SetAddress(addr);

        ptr.Reset();

        return *this;
    }

    static auto GetTypeSize() noexcept -> size_t;

    [[nodiscard]] auto GetAddress() const noexcept -> pointer;
    void SetAddress(pointer addr) noexcept;
    void SetAddress(const Pointer& ptr) noexcept;
    void Reset() noexcept;

    // NOLINTNEXTLINE(google-explicit-constructor), needs for stl containers
    [[nodiscard]] operator const_pointer() const noexcept;
    // NOLINTNEXTLINE(google-explicit-constructor), needs for stl containers
    [[nodiscard]] operator pointer() noexcept;

    [[nodiscard]] explicit operator uintptr_t() const noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] auto operator!() const noexcept -> bool;
    [[nodiscard]] auto IsNull() const noexcept -> bool;

    auto operator=(pointer addr) noexcept -> Pointer&;
    auto operator+=(difference_type ofs) -> Pointer&;
    auto operator++() -> Pointer&;
    auto operator++(int) -> Pointer;
    auto operator-=(difference_type ofs) -> Pointer&;
    auto operator--() -> Pointer&;
    auto operator--(int) -> Pointer;

    [[nodiscard]] auto operator*() const noexcept -> reference;
    [[nodiscard]] auto at() const noexcept -> reference;
    [[nodiscard]] auto operator[](size_t index) const noexcept -> reference;
    [[nodiscard]] auto at(size_t index) const noexcept -> reference;
    [[nodiscard]] auto operator->() const -> pointer;

    [[nodiscard]] static auto pointer_to(adapted_value_type& value) noexcept -> Pointer;

   private:
    void Add(difference_type ofs);
    void Substract(difference_type ofs);

    auto GetGenericAddress() const noexcept -> auto;

    auto CalculateOffset(pointer addr) const noexcept -> difference_type;

    void Copy(const Pointer& ptr) noexcept;
    void Move(Pointer& ptr) noexcept;

    void SetEmptyAddress() noexcept;
    static auto IsValidOffset(difference_type ofs) noexcept -> bool;

   private:
    static constexpr difference_type kInvalidOffset{1};

    difference_type ofs_{kInvalidOffset};
};

template <typename T>
using ConstPointer = Pointer<const T>;

template <typename T>
using ConstConstPointer = const Pointer<const T>;

// Pointer implementation

template <typename T>
Pointer<T>::Pointer() noexcept
{
}

template <typename T>
Pointer<T>::Pointer(pointer addr) noexcept
{
    SetAddress(addr);
}

template <typename T>
Pointer<T>::Pointer(std::nullptr_t) noexcept : Pointer{}
{
}

template <typename T>
Pointer<T>::Pointer(const Pointer& ptr) noexcept
{
    SetAddress(ptr);
}

template <typename T>
Pointer<T>::Pointer(Pointer&& ptr) noexcept
{
    Move(ptr);
}

template <typename T>
auto Pointer<T>::operator=(const Pointer& ptr) noexcept -> Pointer&
{
    Copy(ptr);
    return *this;
}

template <typename T>
auto Pointer<T>::operator=(Pointer&& ptr) noexcept -> Pointer&
{
    Move(ptr);
    return *this;
}

template <typename T>
auto Pointer<T>::operator=(std::nullptr_t) noexcept -> Pointer&
{
    Reset();
    return *this;
}

template <typename T>
auto Pointer<T>::GetAddress() const noexcept -> pointer
{
    if (IsNull())
        return nullptr;

    auto* addr = reinterpret_cast<pointer>(reinterpret_cast<intptr_t>(this) + ofs_);

    if constexpr (!std::is_void_v<value_type>) {
        return std::launder(addr);
    } else {
        return addr;
    }
}

template <typename T>
void Pointer<T>::SetAddress(const Pointer& ptr) noexcept
{
    SetAddress(ptr.GetAddress());
}

template <typename T>
void Pointer<T>::SetAddress(pointer addr) noexcept
{
    if (addr != nullptr) {
        auto ofs = CalculateOffset(addr);

        bool ofs_valid = IsValidOffset(ofs);
        assert(ofs_valid);

        if (ofs_valid)
            ofs_ = ofs;
        else 
            SetEmptyAddress();
    } else {
        SetEmptyAddress();
    }
}

template <typename T>
auto Pointer<T>::CalculateOffset(pointer addr) const noexcept -> difference_type
{
    assert(addr != nullptr);
    return (reinterpret_cast<intptr_t>(addr) - reinterpret_cast<intptr_t>(this));
}

template <typename T>
auto Pointer<T>::IsNull() const noexcept -> bool
{
    return !IsValidOffset(ofs_);
}

template <typename T>
void Pointer<T>::SetEmptyAddress() noexcept
{
    ofs_ = 1;
}

template <typename T>
auto Pointer<T>::IsValidOffset(difference_type ofs) noexcept -> bool
{
    return !((ofs > 0) && (ofs < static_cast<difference_type>(sizeof(uintptr_t))));
}

template <typename T>
Pointer<T>::operator bool() const noexcept
{
    return !IsNull();
}

template <typename T>
auto Pointer<T>::operator!() const noexcept -> bool
{
    return IsNull();
}

template <typename T>
auto Pointer<T>::GetTypeSize() noexcept -> size_t
{
    return sizeof(adapted_value_type);
}

template <typename T>
Pointer<T>::operator const_pointer() const noexcept
{
    return GetAddress();
}

template <typename T>
Pointer<T>::operator pointer() noexcept
{
    return GetAddress();
}

template <typename T>
void Pointer<T>::Reset() noexcept
{
    SetAddress(nullptr);
}

template <typename T>
Pointer<T>::operator uintptr_t() const noexcept
{
    return reinterpret_cast<uintptr_t>(GetAddress());
}

template <typename T>
auto Pointer<T>::operator=(pointer addr) noexcept -> Pointer&
{
    this->SetAddress(addr);
    return *this;
}

template <typename T>
auto Pointer<T>::operator+=(difference_type ofs) -> Pointer&
{
    this->Add(ofs);
    return *this;
}

template <typename T>
auto Pointer<T>::operator++() -> Pointer&
{
    return operator+=(1);
}

template <typename T>
auto Pointer<T>::operator++(int) -> Pointer
{
    Pointer ptr{*this};
    this->Add(1);

    return ptr;
}

template <typename T>
auto Pointer<T>::operator-=(difference_type ofs) -> Pointer&
{
    this->Substract(ofs);
    return *this;
}

template <typename T>
auto Pointer<T>::operator--() -> Pointer&
{
    return operator-=(1);
}

template <typename T>
auto Pointer<T>::operator--(int) -> Pointer
{
    Pointer ptr{*this};
    this->Substract(1);

    return ptr;
}

template <typename T>
auto Pointer<T>::operator*() const noexcept -> reference
{
    auto* addr = this->GetAddress();
    assert(addr != nullptr);

    return *addr;
}

template <typename T>
auto Pointer<T>::at() const noexcept -> reference
{
    auto* addr = this->GetAddress();
    if (addr != nullptr)
        return *addr;

    throw std::logic_error("Can't get value: null pointer");
}

template <typename T>
auto Pointer<T>::operator[](size_t index) const noexcept -> reference
{
    auto* addr = this->GetAddress();
    assert(addr != nullptr);

    return addr[index];
}

template <typename T>
auto Pointer<T>::at(size_t index) const noexcept -> reference
{
    auto* addr = this->GetAddress();
    if (addr != nullptr)
        return addr[index];

    throw std::logic_error("Can't get value by the index: null pointer");
}

template <typename T>
auto Pointer<T>::operator->() const -> pointer
{
    auto* addr = this->GetAddress();
    if (addr != nullptr)
        return addr;

    throw std::logic_error("Null pointer");
}

template <typename T>
auto Pointer<T>::pointer_to(adapted_value_type& value) noexcept -> Pointer
{
    return Pointer(&value);
}

template <typename T>
void Pointer<T>::Add(difference_type ofs)
{
    if (ofs == 0)
        return;

    bool is_null = IsNull();
    assert(!is_null);
    if (is_null)
        throw std::logic_error("Null pointer");

    this->SetAddress(GetGenericAddress() + ofs);
}

template <typename T>
void Pointer<T>::Substract(difference_type ofs)
{
    if (ofs == 0)
        return;

    bool is_null = IsNull();
    assert(!is_null);
    if (is_null)
        throw std::logic_error("Pointer is null");

    this->SetAddress(this->GetGenericAddress() - ofs);
}

template <typename T>
auto Pointer<T>::GetGenericAddress() const noexcept -> auto
{
    auto addr = this->GetAddress();

    return static_cast<typename std::conditional_t<
        !std::is_void_v<value_type>, pointer,
        typename std::conditional_t<std::is_const_v<T>, const char*, char*>>>(addr);
}

template <typename T>
void Pointer<T>::Copy(const Pointer& ptr) noexcept
{
    if (this == &ptr)
        return;
    SetAddress(ptr);
}

template <typename T>
void Pointer<T>::Move(Pointer& ptr) noexcept
{
    if (this == &ptr)
        return;
    SetAddress(ptr);
    ptr.Reset();
}

// Additional utility functions

template <typename T>
auto SME_EXPORT operator+(Pointer<T> lhs,
                          typename Pointer<T>::difference_type ofs) noexcept -> Pointer<T>
{
    lhs += ofs;
    return lhs;
}

template <typename T>
auto SME_EXPORT operator+(typename Pointer<T>::difference_type ofs,
                          Pointer<T> rhs) noexcept -> Pointer<T>
{
    rhs += ofs;
    return rhs;
}

template <typename T>
auto SME_EXPORT operator-(Pointer<T> lhs,
                          typename Pointer<T>::difference_type ofs) noexcept -> Pointer<T>
{
    lhs -= ofs;
    return lhs;
}

template <typename T, typename U>
auto SME_EXPORT operator==(const Pointer<T>& lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) ==
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator==(const Pointer<T>& lhs, U* rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) ==
            reinterpret_cast<uintptr_t>(rhs));
}

template <typename T, typename U>
auto SME_EXPORT operator==(T* lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs) ==
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T>
auto SME_EXPORT operator==(const Pointer<T>& lhs, std::nullptr_t) noexcept -> bool
{
    return lhs.GetAddress() == nullptr;
}

template <typename T, typename U>
auto SME_EXPORT operator!=(const Pointer<T>& lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

template <typename T, typename U>
auto SME_EXPORT operator!=(const Pointer<T>& lhs, U* rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

template <typename T, typename U>
auto SME_EXPORT operator!=(T* lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

template <typename T>
auto SME_EXPORT operator!=(const Pointer<T>& lhs, std::nullptr_t) noexcept -> bool
{
    return lhs.GetAddress() != nullptr;
}

template <typename T, typename U>
auto SME_EXPORT operator<(const Pointer<T>& lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) <
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator<(const Pointer<T>& lhs, U* rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) <
            reinterpret_cast<uintptr_t>(rhs));
}

template <typename T, typename U>
auto SME_EXPORT operator<(T* lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs) <
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator<=(const Pointer<T>& lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) <=
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator<=(T* lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs) <=
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator<=(const Pointer<T>& lhs, U* rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) <=
            reinterpret_cast<uintptr_t>(rhs));
}

template <typename T, typename U>
auto SME_EXPORT operator>(const Pointer<T>& lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) >
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator>(const T* lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs) >
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator>(const Pointer<T>& lhs, U* rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) >
            reinterpret_cast<uintptr_t>(rhs));
}

template <typename T, typename U>
auto SME_EXPORT operator>=(const Pointer<T>& lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) >=
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

template <typename T, typename U>
auto SME_EXPORT operator>=(const Pointer<T>& lhs, U* rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs.GetAddress()) >=
            reinterpret_cast<uintptr_t>(rhs));
}

template <typename T, typename U>
auto SME_EXPORT operator>=(T* lhs, const Pointer<U>& rhs) noexcept -> bool
{
    return (reinterpret_cast<uintptr_t>(lhs) >=
            reinterpret_cast<uintptr_t>(rhs.GetAddress()));
}

// NOLINTEND(cppcoreguidelines-pro-type-const-cast,
// cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-pointer-arithmetic)

}  // namespace sme

#endif  // SME_MEM_PTR_H
