#ifndef SME_MEM_SPACE_MANIP_H
#define SME_MEM_SPACE_MANIP_H

#include <cstdint>
#include <functional>
#include <utility>

#include "sme/internal/mem_space_block.h"
#include "sme/mem_ptr.h"

namespace sme {

using MemorySpaceBlockMatcher =
    std::function<bool(const MemorySpaceBlock& block, MemorySpaceBlock::Size block_size)>;

class MemorySpaceManipulator final {
   public:
    using Block = MemorySpaceBlock;
    using Size = Block::Size;
    using Position = Block::Position;

   public:
    MemorySpaceManipulator(const Pointer<void>& mem, size_t size, bool zeroed = true);

    auto GetBaseAddress() const noexcept -> Pointer<void>;
    auto GetCapacity() const noexcept -> size_t;

    auto UniteBlocks(Block& left_block, Block& right_block) noexcept -> bool;
    [[nodiscard]] auto UniteFreeBlocks(Block& start_block) noexcept -> Block*;
    auto SplitBlock(Block& src_block, Size primary_block_size) noexcept -> Block&;
    auto FreeBlock(const Pointer<void>& ptr) noexcept -> bool;

    auto GetFirstBlock() const noexcept -> Block&;
    auto GetBlock(Position pos) noexcept -> Block&;
    auto GetNextBlock(const Block& block) noexcept -> Block&;
    auto GetPreviousBlock(const Block& block) noexcept -> Block&;
    auto GetBlockPosition(const Block& block) const noexcept -> Position;
    auto GetBlockDataSize(const Block& block) const noexcept -> Size;

    auto GetBlockByDataAddress(const Pointer<void>& data_addr) noexcept -> Block*;
    auto IsLocationValid(const Block& block) const noexcept -> bool;

    [[nodiscard]] auto FindFreeBlock(Block& start_block,
                                     Size block_size,
                                     const MemorySpaceBlockMatcher& matcher,
                                     bool unite_free = false) noexcept -> Block*;

   private:
    [[nodiscard]] auto CreateBlock(Position pos, Size block_size, Position prev_block_pos)
        -> Block&;

    auto GetAddress(Position pos) const noexcept -> char*;

   private:
    Pointer<char> mem_;
    size_t mem_size_{};

    Pointer<Block> first_block_;
};

constexpr auto IsSuitableForAllocation(const MemorySpaceBlock& block,
                                       MemorySpaceBlock::Size block_size) noexcept -> bool
{
    return (block.free && block_size <= block.size);
}

}  // namespace sme

#endif  // SME_MEM_SPACE_MANIP_H
