#include "sme/mem_space.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

#include "sme/internal/mem_space_manip.h"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-pro-type-reinterpret-cast)

namespace sme {

namespace {

void CheckPointerForNull(const Pointer<void>& ptr)
{
    if (!ptr)
        throw std::invalid_argument("Pointer is null");
}

void CheckSizeForZero(size_t size)
{
    if (size == 0)
        throw std::invalid_argument("Allocation size must be greater 0");
}

auto FindBlock(MemorySpaceManipulator& mem_manip, const Pointer<void>& ptr)
    -> MemorySpaceBlock&
{
    auto* block = mem_manip.GetBlockByDataAddress(ptr);
    if (block == nullptr)
        throw std::invalid_argument("Invalid memory pointer");
    return *block;
}

auto FindAllocatedBlock(MemorySpaceManipulator& mem_manip, const Pointer<void>& ptr)
    -> MemorySpaceBlock&
{
    auto& block = FindBlock(mem_manip, ptr);
    if (block.free)
        throw std::invalid_argument("Memory pointer addresses unallocated memory");
    return block;
}

constexpr uint64_t kMemorySpaceCheckTypeId{0x7D857EF281B01};

const MemorySpaceBlockMatcher g_free_block_matcher{&IsSuitableForAllocation};

}  // namespace

MemorySpace::MemorySpace(const Pointer<void>& mem,
                         size_t size,
                         Synchronizer::Type sync_type)
    : kTypeCheckValue{kMemorySpaceCheckTypeId}, sync_{sync_type}, mem_manip_{mem, size},
      curr_block_{&mem_manip_.GetFirstBlock()}
{
}

auto MemorySpace::IsValidObjectId(const MemorySpace& obj) noexcept -> bool
{
    return obj.kTypeCheckValue == kMemorySpaceCheckTypeId;
}

auto MemorySpace::IsValid() const noexcept -> bool
{
    return GetBaseAddress() != nullptr;
}

auto MemorySpace::GetBaseAddress() const noexcept -> Pointer<void>
{
    return mem_manip_.GetBaseAddress();
}

auto MemorySpace::GetCapacity() const noexcept -> size_t
{
    return mem_manip_.GetCapacity();
}

auto MemorySpace::Allocate(size_t size) -> Pointer<void>
{
    CheckSizeForZero(size);

    auto block_size = MemorySpaceBlock::CalculateBlockSize(size);
    const auto& matcher{g_free_block_matcher};

    std::lock_guard lg{sync_};

    Block* suitable_block =
        mem_manip_.FindFreeBlock(*curr_block_, block_size, matcher, true);
    if (suitable_block == nullptr)
        return {};

    if (suitable_block->size > (block_size + MemorySpaceBlock::GetMinBlockSize())) {
        auto& new_free_block = mem_manip_.SplitBlock(*suitable_block, block_size);
        curr_block_ = &new_free_block;
    } else {
        curr_block_ = suitable_block;
    }

    suitable_block->free = false;

    return Pointer<void>{suitable_block->data};
}

auto MemorySpace::AllocateAtLeast(size_t size) -> std::pair<Pointer<void>, size_t>
{
    CheckSizeForZero(size);

    auto block_size = MemorySpaceBlock::CalculateBlockSize(size);
    const auto& matcher{g_free_block_matcher};

    std::lock_guard lg{sync_};

    Block* suitable_block =
        mem_manip_.FindFreeBlock(*curr_block_, block_size, matcher, true);
    if (suitable_block == nullptr)
        return {};

    suitable_block->free = false;

    curr_block_ = suitable_block;

    auto data_size = mem_manip_.GetBlockDataSize(*suitable_block);
    return {Pointer<void>{suitable_block->data}, data_size};
}

auto MemorySpace::Resize(Pointer<void> ptr, size_t new_size) -> bool
{
    CheckPointerForNull(ptr);
    if (new_size == 0)
        return false;

    std::lock_guard lg{sync_};

    Block& src_block = GetAllocatedBlock(ptr);
    auto new_block_size = MemorySpaceBlock::CalculateBlockSize(new_size);

    if (new_block_size == src_block.size)
        return true;

    bool res{};
    if (new_block_size > src_block.size)
        res = ExpandBlock(src_block, new_block_size);
    else
        res = ShrinkBlock(src_block, new_block_size);
    return res;
}

auto MemorySpace::ExpandBlock(Block& block, BlockSize block_size) noexcept -> bool
{
    Block& next_block = mem_manip_.GetNextBlock(block);
    if (!next_block.free || !IsSequentialBlocks(block, next_block))
        return false;

    const auto* free_block = mem_manip_.UniteFreeBlocks(next_block);
    if (free_block != nullptr && Intersects(*free_block, *curr_block_))
        curr_block_ = &block;

    auto unified_block_size = block.size + next_block.size;
    if (block_size > unified_block_size)
        return false;

    mem_manip_.UniteBlocks(block, next_block);

    if (block_size < (unified_block_size - kMemorySpaceBlockHeaderSize))
        mem_manip_.SplitBlock(block, block_size);

    return true;
}

auto MemorySpace::ShrinkBlock(Block& block, BlockSize block_size) noexcept -> bool
{
    if ((block.size - block_size) <= kMemorySpaceBlockHeaderSize)
        return false;

    Block& next_block = mem_manip_.SplitBlock(block, block_size);

    const auto* free_block = mem_manip_.UniteFreeBlocks(next_block);
    if (free_block != nullptr && Intersects(*free_block, *curr_block_))
        curr_block_ = &block;

    return true;
}

auto MemorySpace::Deallocate(Pointer<void>& ptr) noexcept -> bool
{
    if (!ptr)
        return true;

    std::lock_guard lg{sync_};

    if (!mem_manip_.FreeBlock(ptr))
        return false;

    ptr.Reset();
    return true;
}

auto MemorySpace::Deallocate(Pointer<void>&& ptr) noexcept -> bool
{
    return Deallocate(ptr);
}

auto MemorySpace::GetBlock(const Pointer<void>& ptr) const -> const Block&
{
    return FindBlock(mem_manip_, ptr);
}

auto MemorySpace::GetBlock(Pointer<void>& ptr) -> Block&
{
    return FindBlock(mem_manip_, ptr);
}

auto MemorySpace::GetAllocatedBlock(const Pointer<void>& data_ptr) const -> const Block&
{
    return FindAllocatedBlock(mem_manip_, data_ptr);
}

auto MemorySpace::GetAllocatedBlock(Pointer<void>& ptr) -> Block&
{
    return FindAllocatedBlock(mem_manip_, ptr);
}

auto MemorySpace::Intersects(const MemorySpaceBlock& block1,
                             const MemorySpaceBlock& block2) noexcept -> bool
{
    auto pos1 = mem_manip_.GetBlockPosition(block1);
    auto pos2 = mem_manip_.GetBlockPosition(block1);

    bool intersected{};

    if (pos1 < pos2) {
        intersected = (pos2 < (pos1 + block1.size));
    } else if (pos2 < pos1) {
        intersected = (pos1 < (pos2 + block2.size));
    } else {
        intersected = true;
    }

    return intersected;
}

auto MemorySpace::GetAllocationInfo(const Pointer<void>& ptr) const
    -> std::optional<AllocationInfo>
{
    std::lock_guard lg{sync_};

    auto alloc_info_iter = std::find_if(
        begin(), end(),
        [&ptr](const AllocationInfo& alloc_info) { return (alloc_info.data == ptr); });

    return (alloc_info_iter != end() ? std::optional<AllocationInfo>{*alloc_info_iter}
                                     : std::optional<AllocationInfo>{});
}

auto MemorySpace::begin() const noexcept -> Iterator
{
    return Iterator{Pointer<MemorySpaceManipulator>{&mem_manip_}};
}

auto MemorySpace::end() const noexcept -> Iterator
{
    return Iterator{};
}

// MemorySpace::Iterator class

MemorySpace::Iterator::Iterator(Pointer<MemorySpaceManipulator> mem_manip)
    : mem_manip_{std::move(mem_manip)}, iter_block_{&mem_manip_->GetFirstBlock()}
{
}

auto MemorySpace::Iterator::operator*() const -> value_type
{
    return GetValue();
}

auto MemorySpace::Iterator::GetValue() const -> value_type
{
    if (iter_block_ == nullptr)
        throw std::logic_error("Invalid iterator state");

    return (iter_block_ != nullptr)
               ? MemorySpace::AllocationInfo{.block = iter_block_,
                                             .data = iter_block_->data,
                                             .position = mem_manip_->GetBlockPosition(
                                                 *iter_block_),
                                             .size = iter_block_->size,
                                             .free = iter_block_->free}
               : MemorySpace::AllocationInfo{};
}

MemorySpace::Iterator::operator bool() const noexcept
{
    return (iter_block_ != nullptr);
}

auto MemorySpace::Iterator::operator!() const noexcept -> bool
{
    return (iter_block_ == nullptr);
}

auto MemorySpace::Iterator::operator++() -> Iterator&
{
    if (iter_block_ != nullptr) {
        auto& next_block = mem_manip_->GetNextBlock(*iter_block_);
        if (!IsSameBlock(next_block, mem_manip_->GetFirstBlock()))
            iter_block_ = &next_block;
        else
            iter_block_ = nullptr;
    }

    return *this;
}

auto MemorySpace::Iterator::operator++(int) -> Iterator
{
    Iterator res_iter{*this};

    if (iter_block_ != nullptr) {
        auto& next_block = mem_manip_->GetNextBlock(*iter_block_);
        if (!IsSameBlock(next_block, mem_manip_->GetFirstBlock()))
            iter_block_ = &next_block;
        else
            iter_block_ = nullptr;
    }

    return res_iter;
}

auto MemorySpace::Iterator::operator==(const Iterator& other) const noexcept -> bool
{
    return iter_block_ == other.iter_block_;
}

auto MemorySpace::Iterator::operator!=(const Iterator& other) const noexcept -> bool
{
    return !(*this == other);
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-pro-type-reinterpret-cast)
