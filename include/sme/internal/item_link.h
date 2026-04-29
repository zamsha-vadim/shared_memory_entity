#ifndef SME_INTERNAL_ITEM_LINK_H
#define SME_INTERNAL_ITEM_LINK_H

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

#include "sme/internal/ref_count.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-type-const-cast,)

namespace sme {

using ObjectOffset = std::ptrdiff_t;
using UseCounter = int16_t;

constexpr auto kDWCASAlign{16U};

struct alignas(kDWCASAlign) ItemLink final {
    ObjectOffset basic{0};
    ObjectOffset next{0};
};

auto operator==(const ItemLink& lhs, const ItemLink& rhs) noexcept -> bool;
auto operator!=(const ItemLink& lhs, const ItemLink& rhs) noexcept -> bool;

struct ItemDescriptor final {
    std::atomic<ItemLink> link{};
    std::atomic<uint16_t> use_counter{0};
};

template <typename, typename = std::void_t<>>
struct HasItemDescriptor : std::false_type {};

template <typename T>
struct HasItemDescriptor<T, std::void_t<decltype(std::declval<T>().GetItemDescriptor())>>
    : std::true_type {};

auto SME_EXPORT GetNegativeOffsetFlag() noexcept -> uint64_t;

auto SME_EXPORT ExtractOffset(const ObjectOffset& ofs_ptr) noexcept -> ObjectOffset;

auto SME_EXPORT ExtractUseCounter(ObjectOffset ofs_ptr) noexcept -> UseCounter;
auto SME_EXPORT IncreaseUseCounter(ObjectOffset& ofs_ptr, UseCounter value) noexcept
    -> UseCounter;

template <typename BaseT, typename ObjT>
auto SME_EXPORT GetObjectOffset(const BaseT* base, const ObjT* obj) noexcept
    -> ObjectOffset
{
    if (base == nullptr || obj == nullptr)
        return 0;

    const auto* base_addr = reinterpret_cast<const std::byte*>(base);
    const auto* obj_addr = reinterpret_cast<const std::byte*>(obj);

    ObjectOffset ofs_ptr{};

    if (obj_addr >= base_addr) {
        ofs_ptr = obj_addr - base_addr;
    } else {
        ofs_ptr = base_addr - obj_addr;
        ofs_ptr |= GetNegativeOffsetFlag();
    }

    return ofs_ptr;
}

template <typename BaseT, typename ObjT>
auto SME_EXPORT GetObjectAddress(const BaseT* base, ObjectOffset ofs_ptr) noexcept
    -> ObjT*
{
    if (base == nullptr)
        return nullptr;

    auto ofs = ExtractOffset(ofs_ptr);
    if (ofs == 0) {
        if constexpr (std::is_same_v<BaseT, ObjT>)
            return const_cast<BaseT*>(base);
        else
            return nullptr;
    }

    const auto* base_addr = reinterpret_cast<const std::byte*>(base);
    std::byte* obj_addr = const_cast<std::byte*>(base_addr + ofs);

    return std::launder(reinterpret_cast<ObjT*>(obj_addr));
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-type-const-cast,)

#endif  // SME_INTERNAL_ITEM_LINK_H
