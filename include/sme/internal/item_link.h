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

constexpr auto kDWCASAlign{16U};

struct alignas(kDWCASAlign) ItemLink final {
    ObjectOffset basic{0};
    ObjectOffset next{0};
};

template <typename, typename = std::void_t<>>
struct HasItemLink : std::false_type {};

template <typename T>
struct HasItemLink<T, std::void_t<decltype(std::declval<T>().GetItemLink())>>
    : std::true_type {};

template <typename BaseT, typename ObjT>
auto GetObjectOffset(const BaseT* base, const ObjT* obj) noexcept -> ObjectOffset
{
    if (base == nullptr || obj == nullptr)
        return 0;

    auto* base_addr = reinterpret_cast<std::byte*>(const_cast<BaseT*>(base));
    auto obj_addr = reinterpret_cast<const std::byte*>(obj);

    return (obj_addr - base_addr);
}

template <typename BaseT, typename ObjT>
auto GetObjectAddress(const BaseT* base, ObjectOffset ofs_ptr) noexcept -> ObjT*
{
    if (base == nullptr || ofs_ptr == 0)
        return nullptr;

    auto* base_addr = reinterpret_cast<std::byte*>(const_cast<BaseT*>(base));
    auto* obj_addr = base_addr + ofs_ptr;

    return std::launder(reinterpret_cast<ObjT*>(obj_addr));
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-type-const-cast,)

#endif  // SME_INTERNAL_ITEM_LINK_H
