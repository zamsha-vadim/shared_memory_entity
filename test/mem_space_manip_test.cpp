#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

#include "sme/internal/mem_space_manip.h"

namespace {

constexpr size_t kSomeSpaceSize{1024UL * 128 + 7};
std::array<char, kSomeSpaceSize> some_memory{};

auto CreateMemorySpaceManipulator() -> sme::MemorySpaceManipulator
{
    some_memory.fill(0);
    return sme::MemorySpaceManipulator{sme::Pointer<void>{some_memory.data()},
                                       some_memory.size()};
}

auto IsBlockSizeAligned(sme::MemorySpaceBlock::Size size) noexcept -> bool
{
    return (size % sme::kMemorySpaceBlockAlign) == 0;
}

}  // namespace

TEST(MemorySpaceManipulatorTest, TestValidConstructor)
{
    sme::MemorySpaceManipulator manip{sme::Pointer{some_memory.data()},
                                      some_memory.size()};

    EXPECT_LE(manip.GetCapacity(), some_memory.size());
    EXPECT_TRUE(IsBlockSizeAligned(manip.GetCapacity()));
}

TEST(MemorySpaceManipulatorTest, TestConstructorForSmallestValidSize)
{
    alignas(
        sme::kMemorySpaceBlockAlign) char mem[sme::MemorySpaceBlock::GetMinBlockSize()];

    EXPECT_NO_THROW((sme::MemorySpaceManipulator{sme::Pointer{mem},
                                                 sme::MemorySpaceBlock::GetMinBlockSize()}));
}

TEST(MemorySpaceManipulatorTest, TestConstructorForSmallestInvalidSize)
{
    EXPECT_THROW((sme::MemorySpaceManipulator{sme::Pointer{some_memory.data()},
                                              sme::MemorySpaceBlock::GetMinBlockSize() - 1}),
                 std::invalid_argument);
}

TEST(MemorySpaceManipulatorTest, TestConstructorForEmptyArguments)
{
    EXPECT_THROW((sme::MemorySpaceManipulator{sme::Pointer{some_memory.data()}, 0}),
                 std::invalid_argument);
    EXPECT_THROW((sme::MemorySpaceManipulator{{}, sme::MemorySpaceBlock::GetMinBlockSize()}),
                 std::invalid_argument);
}

TEST(MemorySpaceManipulatorTest, TestConstructorForNotAlignedMemory)
{
    constexpr auto kSomeNotAlignValue{11};

    auto not_aligned_addr = some_memory.data() + kSomeNotAlignValue;
    auto size = some_memory.size() - kSomeNotAlignValue;

    sme::MemorySpaceManipulator manip{sme::Pointer{not_aligned_addr}, size};

    EXPECT_EQ(
        static_cast<uintptr_t>(manip.GetBaseAddress()) % sme::kMemorySpaceBlockAlign, 0);
    EXPECT_TRUE(IsBlockSizeAligned(manip.GetCapacity()));
}

TEST(MemorySpaceManipulatorTest, TestGetFirstBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto block_size = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);

    auto& first_block = manip.GetFirstBlock();
    manip.SplitBlock(first_block, block_size);

    EXPECT_EQ(&first_block, &manip.GetFirstBlock());
}

TEST(MemorySpaceManipulatorTest, TestGetBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);

    EXPECT_EQ(&manip.GetBlock(0), &block1);

    auto pos1 = block1.size;
    EXPECT_EQ(&manip.GetBlock(pos1), &block2);

    auto pos2 = block1.size + block2.size;
    EXPECT_EQ(&manip.GetBlock(pos2), &block3);
}

TEST(MemorySpaceManipulatorTest, TestGetNextBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);

    auto& block1 = manip.GetFirstBlock();
    auto& next_block = manip.GetNextBlock(block1);
    EXPECT_EQ(&block1, &next_block);

    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);

    EXPECT_EQ(&block2, &manip.GetNextBlock(block1));
    EXPECT_EQ(&block3, &manip.GetNextBlock(block2));
    EXPECT_EQ(&block1, &manip.GetNextBlock(block3));
}

TEST(MemorySpaceManipulatorTest, TestGetPreviousBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 5);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(1);

    auto& block1 = manip.GetFirstBlock();
    ASSERT_EQ(&manip.GetPreviousBlock(block1), &block1);

    auto& block2 = manip.SplitBlock(block1, size1);
    ASSERT_EQ(&manip.GetPreviousBlock(block1), &block2);

    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);

    EXPECT_EQ(&manip.GetPreviousBlock(block1), &block4);
    EXPECT_EQ(&manip.GetPreviousBlock(block2), &block1);
    EXPECT_EQ(&manip.GetPreviousBlock(block3), &block2);
    EXPECT_EQ(&manip.GetPreviousBlock(block4), &block3);
}

TEST(MemorySpaceManipulatorTest, TestSplitFirstBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto block_size = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);

    auto& src_block = manip.GetFirstBlock();
    auto prev_src_size = src_block.size;

    auto& new_block = manip.SplitBlock(src_block, block_size);

    EXPECT_EQ(new_block.size, prev_src_size - block_size);
    EXPECT_TRUE(new_block.free);
    EXPECT_EQ(new_block.prev_block_pos, manip.GetBlockPosition(src_block));

    EXPECT_EQ(src_block.size, block_size);
    EXPECT_EQ(&manip.GetNextBlock(src_block), &new_block);
}

TEST(MemorySpaceManipulatorTest, TestSplitForOneByteSize)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t one_byte_size{1};
    auto block_size = sme::MemorySpaceBlock::CalculateBlockSize(one_byte_size);
    auto& block1 = manip.GetFirstBlock();

    auto& block2 = manip.SplitBlock(block1, block_size);
    EXPECT_EQ(block1.size, sme::MemorySpaceBlock::GetMinBlockSize());

    manip.SplitBlock(block2, block_size);
    EXPECT_EQ(block2.size, sme::MemorySpaceBlock::GetMinBlockSize());
}

TEST(MemorySpaceManipulatorTest, TestSplitForEqualSizes)
{
    auto manip = CreateMemorySpaceManipulator();

    auto ordinary_block_size = sme::MemorySpaceBlock::CalculateBlockSize(1);
    auto double_block_size = ordinary_block_size * 2;

    (void)manip.SplitBlock(manip.GetFirstBlock(), double_block_size);
    auto& double_block = manip.GetFirstBlock();

    ASSERT_EQ(double_block.size, double_block_size);

    (void)manip.SplitBlock(manip.GetFirstBlock(), ordinary_block_size);
    auto& ordinary_block = manip.GetFirstBlock();

    ASSERT_EQ(ordinary_block.size, ordinary_block_size);
}

TEST(MemorySpaceManipulatorTest, TestSplitInnerBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size21 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size22 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size - 1);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size21);
    auto& block4 = manip.SplitBlock(block2, size22);

    EXPECT_EQ(block1.size, size1);

    EXPECT_EQ(block2.size, size22);
    EXPECT_EQ(&manip.GetNextBlock(block1), &block2);
    EXPECT_EQ(block2.prev_block_pos, manip.GetBlockPosition(block1));

    EXPECT_EQ(block3.prev_block_pos, manip.GetBlockPosition(block4));
    EXPECT_EQ(&manip.GetNextBlock(block4), &block3);

    EXPECT_EQ(block4.prev_block_pos, manip.GetBlockPosition(block2));
    EXPECT_EQ(&manip.GetNextBlock(block4), &block3);
}

TEST(MemorySpaceManipulatorTest, TestSplitLastBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size - 1);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);

    EXPECT_EQ(block1.size, size1);

    EXPECT_EQ(block2.size, size2);
    EXPECT_EQ(&manip.GetNextBlock(block1), &block2);
    EXPECT_EQ(block2.prev_block_pos, manip.GetBlockPosition(block1));

    EXPECT_EQ(block3.size, size3);
    EXPECT_EQ(block3.prev_block_pos, manip.GetBlockPosition(block2));
    EXPECT_EQ(&manip.GetNextBlock(block2), &block3);

    EXPECT_EQ(block4.prev_block_pos, manip.GetBlockPosition(block3));
    EXPECT_EQ(&manip.GetNextBlock(block3), &block4);

    EXPECT_EQ(&manip.GetNextBlock(block4), &block1);
}

TEST(MemorySpaceManipulatorTest, TestUniteUttermostBlocks)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 4);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);

    auto block_size3 = block3.size;
    auto block_size4 = block4.size;

    manip.UniteBlocks(block3, block4);

    ASSERT_EQ(block3.size, block_size3 + block_size4);
    ASSERT_EQ(&manip.GetNextBlock(block3), &block1);
    ASSERT_EQ(&manip.GetPreviousBlock(block1), &block3);
}

TEST(MemorySpaceManipulatorTest, TestUniteInnerBlocks)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size - 1);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);

    auto block_size1 = block1.size;
    auto block_size2 = block2.size;
    auto block_size3 = block3.size;
    auto block_size4 = block4.size;

    manip.UniteBlocks(block2, block3);

    EXPECT_EQ(block2.size, block_size2 + block_size3);
    EXPECT_EQ(&manip.GetNextBlock(block2), &block4);
    EXPECT_EQ(manip.GetBlockPosition(block2), block4.prev_block_pos);
    EXPECT_EQ(block1.size, block_size1);
    EXPECT_EQ(block4.size, block_size4);
}

TEST(MemorySpaceManipulatorTest, TestUniteLastBlocks)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);

    auto block_size1 = block1.size;
    auto block_size2 = block2.size;
    auto block_size3 = block3.size;

    manip.UniteBlocks(block2, block3);

    EXPECT_EQ(block2.size, block_size2 + block_size3);
    EXPECT_EQ(&manip.GetNextBlock(block2), &block1);
    EXPECT_EQ(manip.GetBlockPosition(block1), block2.prev_block_pos);
    EXPECT_EQ(block1.size, block_size1);
}

TEST(MemorySpaceManipulatorTest, TestCalculateBlockSize)
{
    constexpr auto kBlockAlign = sme::kMemorySpaceBlockAlign;

    auto block_size = sme::MemorySpaceBlock::CalculateBlockSize(1);
    EXPECT_EQ(block_size / kBlockAlign, 2);
    EXPECT_EQ(block_size % kBlockAlign, 0);

    block_size = sme::MemorySpaceBlock::CalculateBlockSize(kBlockAlign);
    EXPECT_EQ(block_size / kBlockAlign, 2);
    EXPECT_EQ(block_size % kBlockAlign, 0);

    block_size = sme::MemorySpaceBlock::CalculateBlockSize(kBlockAlign + 1);
    EXPECT_EQ(block_size / kBlockAlign, 3);
    EXPECT_EQ(block_size % kBlockAlign, 0);

    block_size = sme::MemorySpaceBlock::CalculateBlockSize(kBlockAlign * 2);
    EXPECT_EQ(block_size / kBlockAlign, 3);
    EXPECT_EQ(block_size % kBlockAlign, 0);

    block_size = sme::MemorySpaceBlock::CalculateBlockSize(kBlockAlign * 2 + 1);
    EXPECT_EQ(block_size / kBlockAlign, 4);
    EXPECT_EQ(block_size % kBlockAlign, 0);
}

TEST(MemorySpaceManipulatorTest, TestFindFirstFreeBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size / 2);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);

    block1.free = true;
    block2.free = true;
    block3.free = true;
    block4.free = false;

    auto* found_block = manip.FindFreeBlock(block1, size1, sme::IsSuitableForAllocation);
    ASSERT_EQ(&block1, found_block);

    found_block = manip.FindFreeBlock(block3, size1, sme::IsSuitableForAllocation);
    ASSERT_EQ(&block1, found_block);

    found_block = manip.FindFreeBlock(block4, size1, sme::IsSuitableForAllocation);
    ASSERT_EQ(&block1, found_block);
}

TEST(MemorySpaceManipulatorTest, TestFindInnerFreeBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size / 2);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);

    block1.free = false;
    block2.free = true;
    block3.free = true;
    block4.free = false;

    auto* found_block = manip.FindFreeBlock(block1, size1, sme::IsSuitableForAllocation);
    ASSERT_EQ(&block2, found_block);

    found_block = manip.FindFreeBlock(block1, size3, sme::IsSuitableForAllocation);
    ASSERT_EQ(&block2, found_block);

    found_block = manip.FindFreeBlock(block3, size3, sme::IsSuitableForAllocation, true);
    ASSERT_EQ(&block2, found_block);

    found_block = manip.FindFreeBlock(block4, size3, sme::IsSuitableForAllocation);
    ASSERT_EQ(&block2, found_block);
}

TEST(MemorySpaceManipulatorTest, TestFindUnitedInnerFreeBlock)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t some_alloc_size{123};
    auto size1 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size);
    auto size2 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 2);
    auto size3 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size / 2);
    auto size4 = sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size * 3);
    auto size5 =
        sme::MemorySpaceBlock::CalculateBlockSize(some_alloc_size / 2 + 102 + 10);

    auto& block1 = manip.GetFirstBlock();
    auto& block2 = manip.SplitBlock(block1, size1);
    auto& block3 = manip.SplitBlock(block2, size2);
    auto& block4 = manip.SplitBlock(block3, size3);
    auto& block5 = manip.SplitBlock(block4, size4);
    auto& block6 = manip.SplitBlock(block5, size5);

    block1.free = false;
    block2.free = false;
    block3.free = true;
    block4.free = true;
    block5.free = true;
    block6.free = false;

    auto* found_block =
        manip.FindFreeBlock(block1, size3, sme::IsSuitableForAllocation, true);

    ASSERT_TRUE(found_block != nullptr);
    ASSERT_TRUE(sme::IsSameBlock(block3, *found_block));
    ASSERT_TRUE(sme::IsSameBlock(block6, manip.GetNextBlock(*found_block)));

    found_block = manip.FindFreeBlock(block1, found_block->size,
                                      sme::IsSuitableForAllocation, true);
    ASSERT_TRUE(sme::IsSameBlock(block3, *found_block));

    found_block = manip.FindFreeBlock(block1, size2 + size3 + size4 + size5,
                                      sme::IsSuitableForAllocation, true);
    ASSERT_TRUE(found_block == nullptr);
}

TEST(MemorySpaceManipulatorTest, TestFindForMaximumSize)
{
    auto manip = CreateMemorySpaceManipulator();

    size_t max_alloc_size{manip.GetCapacity() - sme::MemorySpaceBlock::GetMinBlockSize()};
    auto max_size = sme::MemorySpaceBlock::CalculateBlockSize(max_alloc_size);

    auto* found_block = manip.FindFreeBlock(manip.GetFirstBlock(), max_size,
                                            sme::IsSuitableForAllocation);
    EXPECT_TRUE(found_block != nullptr);
}
