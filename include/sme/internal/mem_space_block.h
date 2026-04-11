#ifndef SME_MEM_SPACE_BLOCK_H
#define SME_MEM_SPACE_BLOCK_H

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays,
// warnings-as-errors, misc-non-private-member-variables-in-classes,
// cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-pointer-arithmetic)

namespace sme {

constexpr auto kMemorySpaceBlockAlign{alignof(std::max_align_t)};
constexpr auto kMemorySpaceBlockDataAlign{alignof(std::max_align_t)};

struct alignas(kMemorySpaceBlockAlign) MemorySpaceBlock {
    using Size = uint32_t;
    using Position = uint32_t;

    MemorySpaceBlock() = default;
    MemorySpaceBlock(const MemorySpaceBlock&) = delete;
    MemorySpaceBlock(MemorySpaceBlock&&) = delete;
    auto operator=(const MemorySpaceBlock&) -> MemorySpaceBlock& = delete;
    auto operator=(MemorySpaceBlock&&) -> MemorySpaceBlock& = delete;
    ~MemorySpaceBlock() = default;

    static constexpr auto GetMinBlockSize() noexcept -> size_t;
    static constexpr auto GetMinDataSize() noexcept -> size_t;

    static constexpr auto CalculateBlockSize(size_t data_size) noexcept -> Size;
    static constexpr auto CalculateAllocationDataSize(size_t data_size) noexcept -> Size;

    auto GetDataSize() const noexcept -> size_t;

    Size size{0};
    bool free{true};

    Position prev_block_pos{0};

    alignas(kMemorySpaceBlockDataAlign) char data[];
};

constexpr auto kMemorySpaceBlockHeaderSize{offsetof(MemorySpaceBlock, data)};

constexpr auto MemorySpaceBlock::GetMinBlockSize() noexcept -> size_t
{
    constexpr auto kMinNotAlignedSize{kMemorySpaceBlockHeaderSize + 1};

    auto rest = kMinNotAlignedSize % kMemorySpaceBlockAlign;
    return (rest != 0) ? (kMinNotAlignedSize + (kMemorySpaceBlockAlign - rest))
                       : kMinNotAlignedSize;
}

constexpr auto MemorySpaceBlock::GetMinDataSize() noexcept -> size_t
{
    return (GetMinBlockSize() - kMemorySpaceBlockHeaderSize);
}

constexpr auto MemorySpaceBlock::CalculateBlockSize(size_t data_size) noexcept
    -> MemorySpaceBlock::Size
{
    if (data_size == 0)
        return 0;

    auto block_size = kMemorySpaceBlockHeaderSize + data_size;

    auto align_rest = block_size % kMemorySpaceBlockAlign;
    if (align_rest == 0)
        return block_size;

    auto suppl_size = kMemorySpaceBlockAlign - align_rest;
    return (block_size + suppl_size);
}

constexpr auto MemorySpaceBlock::CalculateAllocationDataSize(size_t data_size) noexcept
    -> MemorySpaceBlock::Size
{
    return (CalculateBlockSize(data_size) - kMemorySpaceBlockHeaderSize);
}

constexpr auto IsSameBlock(const MemorySpaceBlock& block1,
                           const MemorySpaceBlock& block2) noexcept -> bool
{
    return &block1 == &block2;
}

inline auto IsSequentialBlocks(const MemorySpaceBlock& left_block,
                               const MemorySpaceBlock& right_block) noexcept -> bool
{
    return (&left_block < &right_block) &&
           (((reinterpret_cast<const char*>(&left_block) + left_block.size) ==
             reinterpret_cast<const char*>(&right_block)) ||
            ((reinterpret_cast<const char*>(&right_block) + right_block.size) ==
             reinterpret_cast<const char*>(&left_block)));
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays,
// warnings-as-errors, misc-non-private-member-variables-in-classes,
// cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-pro-bounds-pointer-arithmetic)

#endif  // SME_MEM_SPACE_BLOCK_H
