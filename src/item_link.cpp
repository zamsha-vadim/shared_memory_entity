#include "sme/internal/item_link.h"

#include <cassert>

namespace sme {

namespace {

constexpr uint64_t kOffsetValueMask{0x0000FFFFFFFFFFFF};
constexpr uint64_t kNegativeOffsetValueFlag{0x8000000000000000};
constexpr uint64_t kCounterValueMask{~(kOffsetValueMask | kNegativeOffsetValueFlag)};
constexpr uint8_t kCounterValueShift{48};

}  // namespace

auto operator==(const ItemLink& lhs, const ItemLink& rhs) noexcept -> bool
{
    return (ExtractOffset(lhs.basic) == ExtractOffset(rhs.basic) &&
            ExtractOffset(lhs.next) == ExtractOffset(rhs.next));
}

auto operator!=(const ItemLink& lhs, const ItemLink& rhs) noexcept -> bool
{
    return !(lhs == rhs);
}

auto GetNegativeOffsetFlag() noexcept -> uint64_t
{
    return kNegativeOffsetValueFlag;
}

auto ExtractOffset(const ObjectOffset& ofs_ptr) noexcept -> ObjectOffset
{
    ObjectOffset ofs = ofs_ptr & kOffsetValueMask;
    if ((ofs_ptr & GetNegativeOffsetFlag()) != 0) 
        ofs = -ofs;
    return ofs;
}

auto ExtractUseCounter(ObjectOffset ofs_ptr) noexcept -> UseCounter
{
    UseCounter counter = ((ofs_ptr & kCounterValueMask) >> kCounterValueShift);
    return counter;
}

auto IncreaseUseCounter(ObjectOffset& ofs_ptr, UseCounter value) noexcept -> UseCounter
{
    assert(value >= 0);

    UseCounter counter = ExtractUseCounter(ofs_ptr);
    counter += value;

    assert(counter >= 0);

    ObjectOffset counter_view = static_cast<ObjectOffset>(counter) << kCounterValueShift;
    bool is_ofs_negative = ((ofs_ptr & GetNegativeOffsetFlag()) != 0);

    ofs_ptr &= kOffsetValueMask;
    ofs_ptr |= counter_view;
    if (is_ofs_negative)
        ofs_ptr |= GetNegativeOffsetFlag();
    
    return counter;
}

}  // namespace sme
