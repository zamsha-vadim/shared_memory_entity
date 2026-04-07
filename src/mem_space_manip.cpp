#include "sme/internal/mem_space_manip.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace sme {

namespace {

[[nodiscard]] auto AlignBaseLocation(Pointer<void> mem, size_t mem_size) noexcept
    -> std::pair<Pointer<void>, size_t>
{
    auto addr_align_rest = static_cast<uintptr_t>(mem) % kMemorySpaceBlockAlign;
    if (addr_align_rest != 0) {
        auto ofs = kMemorySpaceBlockAlign - addr_align_rest;
        if (ofs >= mem_size)
            return {};

        mem += ofs;
        mem_size -= ofs;
    }

    auto size_align_rest = mem_size % kMemorySpaceBlockAlign;
    if (size_align_rest >= mem_size)
        return {};
    mem_size -= size_align_rest;

    return {mem, mem_size};
}

}  // namespace

MemorySpaceManipulator::MemorySpaceManipulator(const Pointer<void>& mem,
                                               size_t size,
                                               bool zeroed)
{
    if (!mem)
        throw std::invalid_argument("Memory pointer is null");

    std::tie(mem_, mem_size_) = AlignBaseLocation(mem, size);

    if (mem_ == nullptr || mem_size_ < MemorySpaceBlock::GetMinBlockSize())
        throw std::invalid_argument("Memory size must be greater " +
                                    std::to_string(MemorySpaceBlock::GetMinBlockSize()));
    if (zeroed)
        std::memset(mem_.GetAddress(), 0, mem_size_);

    auto& first_block = CreateBlock(0, mem_size_, 0);
    first_block_ = &first_block;
}

auto MemorySpaceManipulator::GetBaseAddress() const noexcept -> Pointer<void>
{
    return mem_;
}

auto MemorySpaceManipulator::GetCapacity() const noexcept -> size_t
{
    return mem_size_ - kMemorySpaceBlockHeaderSize;
}

auto MemorySpaceManipulator::CreateBlock(Position pos,
                                         Size block_size,
                                         Position prev_block_pos) -> Block&
{
    assert((pos + block_size) <= mem_size_);
    assert(block_size > kMemorySpaceBlockHeaderSize);
    assert(prev_block_pos < mem_size_);

    auto* location = GetAddress(pos);

    Block* block = new (location) Block;
    block->size = block_size;
    block->prev_block_pos = prev_block_pos;

    return *block;
}

auto MemorySpaceManipulator::UniteBlocks(Block& left_block, Block& right_block) noexcept
    -> bool
{
    assert(IsLocationValid(left_block));
    assert(IsLocationValid(right_block));

    assert(right_block.free);

    if (!right_block.free || IsSameBlock(left_block, right_block) ||
        !IsSequentialBlocks(left_block, right_block))
        return false;

    left_block.size += right_block.size;

    auto& next_right_block = GetNextBlock(right_block);
    next_right_block.prev_block_pos = GetBlockPosition(left_block);

    return true;
}

auto MemorySpaceManipulator::UniteFreeBlocks(Block& src_block) noexcept -> Block*
{
    if (!src_block.free)
        return nullptr;

    auto* first_free_block = &src_block;

    auto* start_block = &src_block;
    auto* next_block = &GetNextBlock(src_block);

    while (next_block->free && !IsSameBlock(*next_block, *start_block)) {
        if (IsSequentialBlocks(*start_block, *next_block)) {
            if (!UniteBlocks(*start_block, *next_block))
                break;
            if (next_block == &src_block)
                first_free_block = start_block;
        } else {
            start_block = next_block;
        }

        next_block = &GetNextBlock(*start_block);
    }

    return first_free_block;
}

auto MemorySpaceManipulator::SplitBlock(Block& src_block,
                                        Size primary_block_size) noexcept -> Block&
{
    assert(IsLocationValid(src_block));

    assert(primary_block_size > kMemorySpaceBlockHeaderSize);
    assert((primary_block_size + MemorySpaceBlock::GetMinBlockSize()) <= src_block.size);

    auto& src_next_block = GetNextBlock(src_block);

    auto src_block_pos = GetBlockPosition(src_block);
    auto new_block_pos = src_block_pos + primary_block_size;
    auto new_block_size = src_block.size - primary_block_size;
    auto& new_block = CreateBlock(new_block_pos, new_block_size, src_block_pos);

    src_block.size = primary_block_size;
    src_next_block.prev_block_pos = new_block_pos;

    return new_block;
}

auto MemorySpaceManipulator::FreeBlock(const Pointer<void>& ptr) noexcept -> bool
{
    Block* block = GetBlockByDataAddress(ptr);
    assert(block != nullptr);
    if (block == nullptr)
        return false;

    block->free = true;
    return true;
}

auto MemorySpaceManipulator::GetFirstBlock() const noexcept -> Block&
{
    return *first_block_;
}

auto MemorySpaceManipulator::GetBlock(Position pos) noexcept -> Block&
{
    auto block_addr = GetAddress(pos);
    assert(block_addr != nullptr);

    auto* block = std::launder(reinterpret_cast<Block*>(block_addr));
    return *block;
}

auto MemorySpaceManipulator::GetAddress(Position pos) const noexcept -> char*
{
    assert(pos < mem_size_);
    assert((pos % kMemorySpaceBlockAlign) == 0);

    auto* addr = reinterpret_cast<char*>(mem_.GetAddress()) + pos;
    assert((reinterpret_cast<uintptr_t>(addr) % kMemorySpaceBlockAlign) == 0);
    return addr;
}

auto MemorySpaceManipulator::GetNextBlock(const Block& block) noexcept -> Block&
{
    assert(IsLocationValid(block));

    const char* next_block_addr = reinterpret_cast<const char*>(&block) + block.size;
    assert(next_block_addr <= (mem_.GetAddress() + mem_size_));

    return (next_block_addr < (mem_.GetAddress() + mem_size_))
               ? *std::launder(
                     reinterpret_cast<Block*>(const_cast<char*>(next_block_addr)))
               : *first_block_;
}

auto MemorySpaceManipulator::GetPreviousBlock(const Block& block) noexcept -> Block&
{
    assert(IsLocationValid(block));

    char* prev_block_addr = mem_.GetAddress() + block.prev_block_pos;
    assert(mem_.GetAddress() <= prev_block_addr &&
           prev_block_addr <
               (mem_.GetAddress() + mem_size_ - kMemorySpaceBlockHeaderSize));

    return *std::launder(reinterpret_cast<Block*>(prev_block_addr));
}

auto MemorySpaceManipulator::GetBlockPosition(const Block& block) const noexcept
    -> Position
{
    assert(IsLocationValid(block));

    const auto* block_addr = reinterpret_cast<const char*>(&block);
    return (block_addr - mem_.GetAddress());
}

auto MemorySpaceManipulator::GetBlockDataSize(const Block& block) const noexcept -> Size
{
    assert(IsLocationValid(block));
    return block.GetDataSize();
}

auto MemorySpaceManipulator::GetBlockByDataAddress(
    const Pointer<void>& data_addr) noexcept -> Block*
{
    if (!data_addr)
        return nullptr;

    auto* data = reinterpret_cast<char*>(data_addr.GetAddress());
    auto* block = std::launder(reinterpret_cast<Block*>(data - offsetof(Block, data)));

    return IsLocationValid(*block) ? block : nullptr;
}

auto MemorySpaceManipulator::IsLocationValid(const Block& block) const noexcept -> bool
{
    const auto* end_mem_addr = mem_.GetAddress() + mem_size_;

    const auto* block_addr = reinterpret_cast<const char*>(&block);
    const auto* end_block_addr = block_addr + block.size;

    return ((mem_.GetAddress() <= block_addr) && (end_block_addr <= end_mem_addr));
}

auto MemorySpaceManipulator::FindFreeBlock(Block& start_block,
                                           Size block_size,
                                           const MemorySpaceBlockMatcher& matcher,
                                           bool unite_free) noexcept -> Block*
{
    assert(IsLocationValid(start_block));

    Block* iter_block = &start_block;

    if (unite_free && start_block.free) {
        auto* prev_block = &GetPreviousBlock(start_block);

        while (prev_block->free && !IsSameBlock(*prev_block, start_block)) {
            iter_block = prev_block;
            prev_block = &GetPreviousBlock(*prev_block);
        }
    }

    assert(iter_block != nullptr);

    do {
        if (iter_block->free) {
            if (unite_free) {
                iter_block = UniteFreeBlocks(*iter_block);
                assert(iter_block != nullptr);
            }

            if (iter_block->free && matcher(*iter_block, block_size))
                return iter_block;
        }

        iter_block = &GetNextBlock(*iter_block);
    } while (iter_block != &start_block);

    return nullptr;
}

}  // namespace sme
