#include <array>

#include <gtest/gtest.h>

#include "sme/internal/mem_domain_block.h"
#include "sme/internal/mem_domain_segment.h"

namespace {

constexpr size_t kSomeSpaceSize{1024UL * 128};

class alignas(sme::kMemoryDomainBlockAlign) SomeMemory {
   public:
    SomeMemory(size_t capacity) : capacity_{capacity} {}

    auto GetCapacity() const noexcept -> size_t { return capacity_; }

    auto GetBaseAddress() const noexcept -> sme::Pointer<const void>
    {
        return {mem_.data()};
    }

    auto GetBaseAddress() noexcept -> sme::Pointer<void>
    {
        return {mem_.data()};
    }

   private:
    size_t capacity_{};
    alignas(
        sme::MemoryDomainUseBlock::kDataAlign) std::array<char, kSomeSpaceSize> mem_{};
};

[[maybe_unused]] auto CreateTestMemory(size_t capacity = kSomeSpaceSize) -> SomeMemory
{
    return SomeMemory{capacity};
}

auto MoveNext(const sme::Pointer<char>& mem, sme::MemoryDomainBlock::Size block_size)
{
    return mem + sme::MemoryDomainSegment::CalculateSegmentSizeForData(block_size);
}

}  // namespace

TEST(MemoryDomainBlockTest, TestCalculateBlockSizeForData)
{
    auto block_size = sme::MemoryDomainUseBlock::CalculateBlockSizeForData(0);
    ASSERT_EQ(block_size, 0);

    block_size = sme::MemoryDomainUseBlock::CalculateBlockSizeForData(
        sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_EQ(block_size, sme::MemoryDomainBlock::GetMinimumBlockSize());

    const std::vector<sme::MemoryDomainBlock::Size> kDataSizes = {
        sme::MemoryDomainUseBlock::GetMinimumDataSize(),
        1,
        2,
        7,
        8,
        9,
        15,
        16,
        17,
        23,
        24,
        25,
        31,
        32,
        33,
        63,
        64,
        65,
        127,
        128,
        129,
        255,
        256,
        257,
        511,
        512,
        513,
        1023,
        1024,
        1025,
        (1024 * 32 - 1),
        (1024 * 32),
        (1024 * 32 + 1),
        (1024 * 64 - 1),
        (1024 * 64),
        (1024 * 64 + 1),
        (1024 * 128 - 1),
        (1024 * 128),
        (1024 * 128 + 1),
        sme::MemoryDomainUseBlock::GetMaximumDataSize() - 1,
        sme::MemoryDomainUseBlock::GetMaximumDataSize()};

    auto block_align{sme::MemoryDomainUseBlock::kDataAlign};

    for (auto data_size : kDataSizes) {
        block_size = sme::MemoryDomainUseBlock::CalculateBlockSizeForData(data_size);

        ASSERT_EQ(block_size % block_align, 0);

        auto block_data_size = block_size - sizeof(sme::MemoryDomainUseBlock);
        ASSERT_GE(block_data_size, sme::MemoryDomainUseBlock::GetMinimumDataSize());
        ASSERT_EQ(block_data_size % sme::MemoryDomainUseBlock::kDataSizeStep, 0);
    }
}

TEST(MemoryDomainBlockTest, TestUseBlockConstructor)
{
    auto mem_realm = CreateTestMemory();
    auto mem_ptr = mem_realm.GetBaseAddress();

    auto some_block_size{sme::MemoryDomainBlock::GetMinimumBlockSize() * 2};
    auto some_prev_block_ofs{sme::MemoryDomainBlock::GetMinimumBlockSize()};

    auto block = new (mem_ptr.GetAddress())
        sme::MemoryDomainUseBlock{some_block_size, some_prev_block_ofs, true};

    ASSERT_EQ(reinterpret_cast<uintptr_t>(block) % sme::kMemoryDomainBlockAlign, 0);

    EXPECT_EQ(block->GetType(), sme::MemoryDomainBlock::Type::kUsed);

    EXPECT_EQ(block->GetBlockSize(), some_block_size);
    EXPECT_EQ(block->GetBlockSize() % sme::MemoryDomainBlock::kBlockAlign, 0);

    EXPECT_FALSE(block->IsFirstBlock());
    EXPECT_TRUE(block->IsLastBlock());
    EXPECT_NE(block->GetPreviousBlock(), nullptr);
    EXPECT_EQ(block->GetNextBlock(), nullptr);
    EXPECT_EQ(block->GetPreviousBlockOffset(), some_prev_block_ofs);

    block->~MemoryDomainUseBlock();
}

TEST(MemoryDomainBlockTest, TestUseBlockData)
{
    auto mem_realm = CreateTestMemory();
    auto mem_ptr = mem_realm.GetBaseAddress();

    auto some_block_size{sme::MemoryDomainBlock::GetMinimumBlockSize() * 2};

    auto block = new (mem_ptr.GetAddress()) sme::MemoryDomainUseBlock{some_block_size};

    EXPECT_GT(block->GetDataSize(), 0);
    EXPECT_LE(block->GetDataSize(),
              block->GetBlockSize() - sizeof(sme::MemoryDomainUseBlock));
    EXPECT_EQ(block->GetDataSize() % sme::MemoryDomainUseBlock::kDataAlign, 0);

    ASSERT_NE(block->GetData(), nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(block->GetData()) %
                  sme::MemoryDomainUseBlock::kDataAlign,
              0);
}

TEST(MemoryDomainBlockTest, TestFreeSmallBlockConstructor)
{
    auto mem_realm = CreateTestMemory();
    auto mem_ptr = mem_realm.GetBaseAddress();

    auto some_prev_block_ofs{sme::MemoryDomainBlock::GetMinimumBlockSize()};

    auto block = new (mem_ptr.GetAddress())
        sme::MemoryDomainFreeSmallBlock{some_prev_block_ofs, true};

    EXPECT_EQ(block->GetType(), sme::MemoryDomainBlock::Type::kFreeSmall);

    EXPECT_EQ(block->GetBlockSize(), sme::MemoryDomainBlock::GetMinimumBlockSize());

    EXPECT_FALSE(block->IsFirstBlock());
    EXPECT_TRUE(block->IsLastBlock());
    EXPECT_NE(block->GetPreviousBlock(), nullptr);
    EXPECT_EQ(block->GetNextBlock(), nullptr);
    EXPECT_EQ(block->GetPreviousBlockOffset(), some_prev_block_ofs);

    block->~MemoryDomainFreeSmallBlock();
}

TEST(MemoryDomainBlockTest, TestFreeGenericBlockConstructor)
{
    auto mem_realm = CreateTestMemory();
    auto mem_ptr = mem_realm.GetBaseAddress();

    auto some_block_size{sme::MemoryDomainBlock::GetMinimumBlockSize() * 2};
    auto some_prev_block_ofs{sme::MemoryDomainBlock::GetMinimumBlockSize() * 3};

    auto block = new (mem_ptr.GetAddress())
        sme::MemoryDomainFreeGenericBlock{some_block_size, some_prev_block_ofs, true};

    EXPECT_EQ(block->GetType(), sme::MemoryDomainBlock::Type::kFreeGeneric);

    EXPECT_EQ(block->GetBlockSize(), some_block_size);

    EXPECT_FALSE(block->IsFirstBlock());
    EXPECT_TRUE(block->IsLastBlock());
    EXPECT_NE(block->GetPreviousBlock(), nullptr);
    EXPECT_EQ(block->GetNextBlock(), nullptr);
    EXPECT_EQ(block->GetPreviousBlockOffset(), some_prev_block_ofs);

    block->~MemoryDomainFreeGenericBlock();
}

TEST(MemoryDomainBlockTest, TestBlockIteration)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    auto block_size1{sme::MemoryDomainBlock::GetMinimumBlockSize() * 3};
    auto block_size3{sme::MemoryDomainBlock::GetMinimumBlockSize() * 2};

    sme::Pointer<sme::MemoryDomainBlock> block1 = new (mem_ptr.GetAddress())
        sme::MemoryDomainUseBlock{block_size1, 0, false};
    ASSERT_TRUE(block1->IsFirstBlock());
    ASSERT_FALSE(block1->IsLastBlock());

    sme::Pointer<sme::MemoryDomainBlock> block2 = new (mem_ptr.GetAddress() + block_size1)
        sme::MemoryDomainFreeSmallBlock{block_size1, false};
    ASSERT_FALSE(block2->IsFirstBlock());
    ASSERT_FALSE(block2->IsLastBlock());

    sme::Pointer<sme::MemoryDomainBlock> block3 =
        new (mem_ptr.GetAddress() + block_size1 + block2->GetBlockSize())
            sme::MemoryDomainUseBlock{block_size3, block2->GetBlockSize(), true};
    ASSERT_FALSE(block2->IsFirstBlock());
    ASSERT_TRUE(block3->IsLastBlock());

    EXPECT_EQ(block1->GetPreviousBlock(), nullptr);
    EXPECT_EQ(block1->GetNextBlock(), block2);

    EXPECT_EQ(block2->GetPreviousBlock(), block1);
    EXPECT_EQ(block2->GetNextBlock(), block3);

    EXPECT_EQ(block3->GetPreviousBlock(), block2);
    EXPECT_EQ(block3->GetNextBlock(), nullptr);

    for (auto iter_block = block1; iter_block != nullptr;
         iter_block = iter_block->GetNextBlock()) {
        ASSERT_TRUE(iter_block <= block3);
    }

    for (auto iter_block = block3; iter_block != nullptr;
         iter_block = iter_block->GetPreviousBlock()) {
        ASSERT_TRUE(iter_block >= block1);
    }
}

TEST(MemoryDomainBlockTest, TestCalculateInitialFreeBlockSize)
{
    auto mem_size{0u};
    auto block_size =
        sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, 0);

    mem_size = 1;
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, 0);

    const auto min_seg_size = sme::MemoryDomainSegment::GetMinimumDataCapacity();

    mem_size = min_seg_size - 1;
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, 0);

    mem_size = min_seg_size;
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, sme::MemoryDomainBlock::GetMinimumBlockSize());

    mem_size = min_seg_size + 1;
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, sme::MemoryDomainBlock::GetMinimumBlockSize());

    mem_size = (min_seg_size + sme::MemoryDomainBlock::GetMinimumBlockSize()) - 1;
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, sme::MemoryDomainBlock::GetMinimumBlockSize());

    mem_size = min_seg_size + sme::MemoryDomainBlock::GetMinimumBlockSize();
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, sme::MemoryDomainBlock::GetMinimumBlockSize() * 2);

    mem_size = (min_seg_size + sme::MemoryDomainBlock::GetMinimumBlockSize()) + 1;
    block_size = sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size);
    EXPECT_EQ(block_size, sme::MemoryDomainBlock::GetMinimumBlockSize() * 2);
}

TEST(MemoryDomainBlockTest, TestFreeBlockSplittable)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    auto free_block_size{sme::MemoryDomainBlock::GetMinimumBlockSize()};

    auto block = new (mem_ptr.GetAddress())
        sme::MemoryDomainFreeGenericBlock{free_block_size};
    EXPECT_FALSE(block->CanBeSplittedForUse(0));
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize()));
    EXPECT_FALSE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() + 1));
    block->~MemoryDomainFreeGenericBlock();

    free_block_size = sme::MemoryDomainBlock::GetMinimumBlockSize() * 2;

    block = new (mem_ptr.GetAddress()) sme::MemoryDomainFreeGenericBlock{free_block_size};
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() - 1));
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize()));
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() +
                                   sme::MemoryDomainUseBlock::GetMinimumDataSize()));
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() * 2));
    EXPECT_FALSE(block->CanBeSplittedForUse(
        sme::MemoryDomainBlock::GetMinimumBlockSize() * 2 + 1));
    EXPECT_FALSE(block->CanBeSplittedForUse(
        sme::MemoryDomainBlock::GetMinimumBlockSize() * 3 - 1));
    EXPECT_FALSE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() * 3));
    block->~MemoryDomainFreeGenericBlock();

    free_block_size = sme::MemoryDomainBlock::GetMinimumBlockSize() * 3;

    block = new (mem_ptr.GetAddress()) sme::MemoryDomainFreeGenericBlock{free_block_size};
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize()));
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() * 2));
    EXPECT_TRUE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() * 3));
    EXPECT_FALSE(block->CanBeSplittedForUse(
        sme::MemoryDomainBlock::GetMinimumBlockSize() * 3 + 1));
    EXPECT_FALSE(
        block->CanBeSplittedForUse(sme::MemoryDomainBlock::GetMinimumBlockSize() * 4));
    EXPECT_FALSE(block->CanBeSplittedForUse(
        sme::MemoryDomainBlock::GetMinimumBlockSize() * 4 - 1));
    block->~MemoryDomainFreeGenericBlock();
}

TEST(MemoryDomainBlockPoolTest, AddOneGenericFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<void> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [free_block, redzone_block] =
        mp.AddFreeMemoryArea(static_cast<void*>(mem_ptr), mem_size);

    ASSERT_TRUE(free_block != nullptr);
    ASSERT_EQ(free_block->GetBlockSize(),
              sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(mem_size));

    EXPECT_EQ(mp.GetFirstFreeGenericBlock(), free_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(redzone_block->GetType() == sme::MemoryDomainBlock::Type::kRedZone);
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == free_block);
    ASSERT_TRUE(redzone_block == free_block->GetNextBlock());
}

TEST(MemoryDomainBlockPoolTest, AddLessSizeOfFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<void> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto greater_mem_size = mem_realm.GetCapacity() / 2;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> greater_block;
    std::tie(greater_block, std::ignore) =
        mp.AddFreeMemoryArea(mem_ptr, greater_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), greater_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    auto less_mem_size = greater_block->GetBlockSize() / 2;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> less_block;
    std::tie(less_block, std::ignore) = mp.AddFreeMemoryArea(
        MoveNext(mem_ptr, greater_block->GetBlockSize()), less_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), less_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    EXPECT_TRUE(less_block->GetLessFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetGreaterFreeBlock() == greater_block);
    EXPECT_TRUE(less_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(greater_block->GetLessFreeBlock() == less_block);
    EXPECT_TRUE(greater_block->GetGreaterFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetNextFreeBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest, AddGreaterSizeOfFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<void> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto less_mem_size = mem_realm.GetCapacity() / 4;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> less_block;
    std::tie(less_block, std::ignore) = mp.AddFreeMemoryArea(mem_ptr, less_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), less_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    auto greater_mem_size = mem_realm.GetCapacity() / 3;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> greater_block;
    std::tie(greater_block, std::ignore) = mp.AddFreeMemoryArea(
        MoveNext(mem_ptr, less_block->GetBlockSize()), greater_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), less_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    EXPECT_TRUE(less_block->GetLessFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetGreaterFreeBlock() == greater_block);
    EXPECT_TRUE(less_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(greater_block->GetLessFreeBlock() == less_block);
    EXPECT_TRUE(greater_block->GetGreaterFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetNextFreeBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest, AddMiddleSizeOfFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<void> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto less_mem_size = mem_realm.GetCapacity() / 10;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> less_block;
    std::tie(less_block, std::ignore) = mp.AddFreeMemoryArea(mem_ptr, less_mem_size);

    auto middle_mem_size = mem_realm.GetCapacity() / 4;
    auto middle_mem = MoveNext(mem_ptr, less_block->GetBlockSize());
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> middle_block;
    std::tie(middle_block, std::ignore) =
        mp.AddFreeMemoryArea(middle_mem, middle_mem_size);

    auto greater_mem_size = mem_realm.GetCapacity() / 2;
    auto greater_mem = MoveNext(mem_ptr, middle_block->GetBlockSize());
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> greater_block;
    std::tie(greater_block, std::ignore) =
        mp.AddFreeMemoryArea(greater_mem, greater_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), less_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    EXPECT_TRUE(less_block->GetLessFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetGreaterFreeBlock() == middle_block);
    EXPECT_TRUE(less_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(middle_block->GetLessFreeBlock() == less_block);
    EXPECT_TRUE(middle_block->GetGreaterFreeBlock() == greater_block);
    EXPECT_TRUE(middle_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(middle_block->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(greater_block->GetLessFreeBlock() == middle_block);
    EXPECT_TRUE(greater_block->GetGreaterFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetNextFreeBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest, AddEqualSizeOfFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto less_mem_size = mem_realm.GetCapacity() / 10;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> less_block;
    std::tie(less_block, std::ignore) = mp.AddFreeMemoryArea(mem_ptr, less_mem_size);

    auto middle_mem_size = mem_realm.GetCapacity() / 8;
    auto middle_mem1 = MoveNext(mem_ptr, less_block->GetBlockSize());
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> middle_block1;
    std::tie(middle_block1, std::ignore) =
        mp.AddFreeMemoryArea(middle_mem1, middle_mem_size);

    auto greater_mem_size = mem_realm.GetCapacity() / 2;
    auto greater_mem = MoveNext(mem_ptr, middle_block1->GetBlockSize());
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> greater_block;
    std::tie(greater_block, std::ignore) =
        mp.AddFreeMemoryArea(greater_mem, greater_mem_size);

    auto middle_mem2 = MoveNext(mem_ptr, greater_block->GetBlockSize());
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> middle_block2;
    std::tie(middle_block2, std::ignore) =
        mp.AddFreeMemoryArea(middle_mem2, middle_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), less_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    EXPECT_TRUE(less_block->GetLessFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetGreaterFreeBlock() == middle_block2);
    EXPECT_TRUE(less_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(less_block->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(middle_block1->GetLessFreeBlock() == nullptr);
    EXPECT_TRUE(middle_block1->GetGreaterFreeBlock() == nullptr);
    EXPECT_TRUE(middle_block1->GetPreviousFreeBlock() == middle_block2);
    EXPECT_TRUE(middle_block1->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(middle_block2->GetLessFreeBlock() == less_block);
    EXPECT_TRUE(middle_block2->GetGreaterFreeBlock() == greater_block);
    EXPECT_TRUE(middle_block2->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(middle_block2->GetNextFreeBlock() == middle_block1);

    EXPECT_TRUE(greater_block->GetLessFreeBlock() == middle_block2);
    EXPECT_TRUE(greater_block->GetGreaterFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(greater_block->GetNextFreeBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest, AddOneSmallSizeOfFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto invalid_mem_size = sme::MemoryDomainSegment::GetMinimumDataCapacity() - 1;
    auto [invalid_block, redzone_block0] =
        mp.AddFreeMemoryArea(mem_ptr, invalid_mem_size);

    ASSERT_TRUE(invalid_block == nullptr);

    auto min_mem_size = sme::MemoryDomainSegment::GetMinimumDataCapacity();
    auto [free_block, redzone_block] = mp.AddFreeMemoryArea(mem_ptr, min_mem_size);

    ASSERT_TRUE(free_block != nullptr);
    ASSERT_EQ(
        free_block->GetBlockSize(),
        sme::MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(min_mem_size));

    EXPECT_EQ(mp.GetFirstFreeGenericBlock(), nullptr);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), free_block);

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(redzone_block->GetType() == sme::MemoryDomainBlock::Type::kRedZone);
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == free_block);
    ASSERT_TRUE(redzone_block == free_block->GetNextBlock());
}

TEST(MemoryDomainBlockPoolTest, AddAnotherFreeSmallSizeOfFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem1 = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto min_mem_size = sme::MemoryDomainSegment::GetMinimumDataCapacity();
    sme::Pointer<sme::MemoryDomainFreeSmallBlock> free_block1;
    std::tie(free_block1, std::ignore) = mp.AddFreeMemoryArea(mem1, min_mem_size);

    ASSERT_TRUE(free_block1 != nullptr);
    ASSERT_EQ(free_block1->GetType(), sme::MemoryDomainBlock::Type::kFreeSmall);

    sme::Pointer<char> mem2 = MoveNext(mem1, min_mem_size);
    sme::Pointer<sme::MemoryDomainFreeSmallBlock> free_block2;
    std::tie(free_block2, std::ignore) = mp.AddFreeMemoryArea(mem2, min_mem_size);

    ASSERT_EQ(free_block2->GetType(), sme::MemoryDomainBlock::Type::kFreeSmall);

    EXPECT_EQ(mp.GetFirstFreeGenericBlock(), nullptr);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), free_block2);

    EXPECT_TRUE(free_block1->GetPreviousFreeBlock() == free_block2);
    EXPECT_TRUE(free_block1->GetNextFreeBlock() == nullptr);

    EXPECT_TRUE(free_block2->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(free_block2->GetNextFreeBlock() == free_block1);
}

TEST(MemoryDomainBlockPoolTest, AddMixedSizesFreeMemory)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto less_mem_size = mem_realm.GetCapacity() / 10;
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> less_block;
    std::tie(less_block, std::ignore) = mp.AddFreeMemoryArea(mem_ptr, less_mem_size);

    ASSERT_TRUE(less_block != nullptr);

    auto small_mem_size = sme::MemoryDomainSegment::GetMinimumDataCapacity();
    auto small_mem = MoveNext(mem_ptr, less_block->GetBlockSize());

    sme::Pointer<sme::MemoryDomainFreeGenericBlock> small_block;
    std::tie(small_block, std::ignore) = mp.AddFreeMemoryArea(small_mem, small_mem_size);

    ASSERT_TRUE(small_block != nullptr);
    ASSERT_EQ(small_block->GetType(), sme::MemoryDomainBlock::Type::kFreeSmall);

    auto greater_mem_size = mem_realm.GetCapacity() / 2;
    auto greater_mem = MoveNext(mem_ptr, small_block->GetBlockSize());
    sme::Pointer<sme::MemoryDomainFreeGenericBlock> greater_block;
    std::tie(greater_block, std::ignore) =
        mp.AddFreeMemoryArea(greater_mem, greater_mem_size);

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), less_block);
    EXPECT_EQ(mp.GetFirstFreeSmallBlock(), small_block);

    ASSERT_TRUE(less_block->GetLessFreeBlock() == nullptr);
    ASSERT_TRUE(less_block->GetGreaterFreeBlock() == greater_block);
    ASSERT_TRUE(less_block->GetPreviousFreeBlock() == nullptr);
    ASSERT_TRUE(less_block->GetNextFreeBlock() == nullptr);

    ASSERT_TRUE(small_block->GetPreviousFreeBlock() == nullptr);
    ASSERT_TRUE(small_block->GetNextFreeBlock() == nullptr);

    ASSERT_TRUE(greater_block->GetLessFreeBlock() == less_block);
    ASSERT_TRUE(greater_block->GetGreaterFreeBlock() == nullptr);
    ASSERT_TRUE(greater_block->GetPreviousFreeBlock() == nullptr);
    ASSERT_TRUE(greater_block->GetNextFreeBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest, AllocateOneGenericBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block] = mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto data_size{mem_size / 4};
    auto used_block = mp.AllocateUseBlock(data_size);

    ASSERT_TRUE(used_block != nullptr);
    ASSERT_EQ(used_block, first_block);
    ASSERT_FALSE(used_block->IsLastBlock());

    ASSERT_TRUE(used_block->GetNextBlock() != nullptr);
    ASSERT_FALSE(used_block->GetNextBlock()->IsLastBlock());
    ASSERT_TRUE(sme::IsFree(*(used_block->GetNextBlock())));

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != first_block);
    ASSERT_EQ(used_block->GetNextBlock(), mp.GetFirstFreeGenericBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_FALSE(mp.GetFirstFreeGenericBlock()->IsLastBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock()->IsLastBlock());
    ASSERT_TRUE(sme::IsRedZone(*mp.GetFirstFreeGenericBlock()->GetNextBlock()));

    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock(), nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock(), nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetPreviousBlock(), used_block);

    ASSERT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);
}

TEST(MemoryDomainBlockPoolTest, ReallocateLastGenericBlockAtEnd)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block0] = mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto data_size1{sme::MemoryDomainUseBlock::GetDataCapacityAt(*first_block) -
                    (sme::MemoryDomainBlock::GetMinimumBlockSize() * 2 -
                     sme::MemoryDomainUseBlock::GetMinimumDataSize())};
    auto used_block1 = mp.AllocateUseBlock(data_size1);

    ASSERT_TRUE(used_block1 != nullptr);

    auto last_used_block =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize() + 1);

    ASSERT_TRUE(last_used_block != nullptr);
    ASSERT_FALSE(last_used_block->IsLastBlock());
    ASSERT_TRUE(last_used_block->GetPreviousBlock() == used_block1);
    ASSERT_TRUE(used_block1->GetNextBlock() == last_used_block);

    auto redzone_block = last_used_block->GetNextBlock();

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(redzone_block == redzone_block0);
    ASSERT_TRUE(sme::IsRedZone(*redzone_block));
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == last_used_block);
    ASSERT_TRUE(redzone_block == last_used_block->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.AllocateUseBlock(1) == nullptr);

    mp.DeallocateUseBlock(last_used_block);

    ASSERT_TRUE(used_block1 != nullptr);
    ASSERT_EQ(used_block1, first_block);
    ASSERT_FALSE(used_block1->IsLastBlock());

    ASSERT_TRUE(used_block1->GetNextBlock() != nullptr);
    ASSERT_FALSE(used_block1->GetNextBlock()->IsLastBlock());

    ASSERT_EQ(used_block1->GetNextBlock(), mp.GetFirstFreeGenericBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != first_block);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetPreviousBlock(), used_block1);

    ASSERT_FALSE(mp.GetFirstFreeGenericBlock()->IsLastBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock()->IsLastBlock());
    ASSERT_TRUE(sme::IsRedZone(*mp.GetFirstFreeGenericBlock()->GetNextBlock()));

    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock(), nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock(), nullptr);

    ASSERT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);

    last_used_block =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize() + 1);

    ASSERT_TRUE(last_used_block != nullptr);
    ASSERT_FALSE(last_used_block->IsLastBlock());
    ASSERT_TRUE(last_used_block->GetPreviousBlock() == used_block1);
    ASSERT_TRUE(used_block1->GetNextBlock() == last_used_block);

    redzone_block = last_used_block->GetNextBlock();

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(sme::IsRedZone(*redzone_block));
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == last_used_block);
    ASSERT_TRUE(redzone_block == last_used_block->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.AllocateUseBlock(1) == nullptr);
}

TEST(MemoryDomainBlockPoolTest, DeallocateOneGenericBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_free_block, redzone_block] = mp.AddFreeMemoryArea(mem_ptr, mem_size);
    auto total_block_size = first_free_block->GetBlockSize();

    auto data_size{mem_size / 4};
    auto used_block = mp.AllocateUseBlock(data_size);
    ASSERT_TRUE(used_block != nullptr);

    mp.DeallocateUseBlock(used_block);

    ASSERT_TRUE(used_block == nullptr);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == first_free_block);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetBlockSize(), total_block_size);
    ASSERT_FALSE(mp.GetFirstFreeGenericBlock()->IsLastBlock());

    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock()->IsLastBlock());
    ASSERT_TRUE(sme::IsRedZone(*mp.GetFirstFreeGenericBlock()->GetNextBlock()));

    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() == nullptr);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest, AllocateOneSmallBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block] = mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto data_size{sme::MemoryDomainUseBlock::GetMinimumDataSize()};
    auto used_block = mp.AllocateUseBlock(data_size);

    ASSERT_TRUE(used_block != nullptr);
    ASSERT_TRUE(used_block == first_block);
    ASSERT_FALSE(used_block->IsLastBlock());

    ASSERT_TRUE(used_block->GetNextBlock() != nullptr);
    ASSERT_FALSE(used_block->GetNextBlock()->IsLastBlock());

    EXPECT_EQ(used_block->GetBlockSize(), sme::MemoryDomainBlock::GetMinimumBlockSize());

    ASSERT_EQ(used_block->GetNextBlock(), mp.GetFirstFreeGenericBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_FALSE(mp.GetFirstFreeGenericBlock()->IsLastBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock()->IsLastBlock());
    ASSERT_TRUE(sme::IsRedZone(*mp.GetFirstFreeGenericBlock()->GetNextBlock()));

    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock(), nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock(), nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetPreviousBlock(), used_block);

    ASSERT_EQ(mp.GetFirstFreeSmallBlock(), nullptr);
}

TEST(MemoryDomainBlockPoolTest, ReallocateLastSmallBlockAtEnd)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block0] = mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto data_size1{sme::MemoryDomainUseBlock::GetDataCapacityAt(*first_block) -
                    (sme::MemoryDomainBlock::GetMinimumBlockSize() * 1)};
    auto used_block1 = mp.AllocateUseBlock(data_size1);

    ASSERT_TRUE(used_block1 != nullptr);

    auto last_used_block =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    ASSERT_TRUE(last_used_block != nullptr);
    ASSERT_FALSE(last_used_block->IsLastBlock());
    ASSERT_TRUE(last_used_block->GetPreviousBlock() == used_block1);
    ASSERT_TRUE(used_block1->GetNextBlock() == last_used_block);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.AllocateUseBlock(1) == nullptr);

    auto redzone_block = last_used_block->GetNextBlock();

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(redzone_block == redzone_block0);
    ASSERT_TRUE(sme::IsRedZone(*redzone_block));
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == last_used_block);
    ASSERT_TRUE(redzone_block == last_used_block->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.AllocateUseBlock(1) == nullptr);

    mp.DeallocateUseBlock(last_used_block);

    ASSERT_TRUE(used_block1 != nullptr);
    ASSERT_EQ(used_block1, first_block);
    ASSERT_FALSE(used_block1->IsLastBlock());

    ASSERT_TRUE(used_block1->GetNextBlock() != nullptr);
    ASSERT_FALSE(used_block1->GetNextBlock()->IsLastBlock());

    ASSERT_EQ(used_block1->GetNextBlock(), mp.GetFirstFreeSmallBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() != first_block);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() != nullptr);
    ASSERT_EQ(mp.GetFirstFreeSmallBlock()->GetPreviousBlock(), used_block1);

    ASSERT_FALSE(mp.GetFirstFreeSmallBlock()->IsLastBlock());
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock()->IsLastBlock());
    ASSERT_TRUE(sme::IsRedZone(*mp.GetFirstFreeSmallBlock()->GetNextBlock()));

    ASSERT_EQ(mp.GetFirstFreeGenericBlock(), nullptr);

    last_used_block =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    ASSERT_TRUE(last_used_block != nullptr);
    ASSERT_FALSE(last_used_block->IsLastBlock());
    ASSERT_TRUE(last_used_block->GetPreviousBlock() == used_block1);
    ASSERT_TRUE(used_block1->GetNextBlock() == last_used_block);

    redzone_block = last_used_block->GetNextBlock();

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(sme::IsRedZone(*redzone_block));
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == last_used_block);
    ASSERT_TRUE(redzone_block == last_used_block->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.AllocateUseBlock(1) == nullptr);
}

TEST(MemoryDomainBlockPoolTest, DeallocateOneSmallBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [free_block, redzone_block] = mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto data_size{sme::MemoryDomainUseBlock::GetMinimumDataSize()};
    auto used_block = mp.AllocateUseBlock(data_size);

    mp.DeallocateUseBlock(used_block);
    ASSERT_TRUE(used_block == nullptr);

    EXPECT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);  // unified to all space
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock() == free_block);
}

TEST(MemoryDomainBlockPoolTest, AllocateFirstGenericAndSmallBlocks)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto data_size1{mem_size / 4};
    auto used_block1 = mp.AllocateUseBlock(data_size1);

    ASSERT_TRUE(used_block1 != nullptr);

    auto data_size2{sme::MemoryDomainUseBlock::GetMinimumDataSize()};
    auto used_block2 = mp.AllocateUseBlock(data_size2);

    ASSERT_TRUE(used_block2 != nullptr);
    EXPECT_EQ(used_block2->GetBlockSize(), sme::MemoryDomainBlock::GetMinimumBlockSize());

    ASSERT_EQ(used_block1->GetNextBlock(), used_block2);
    ASSERT_EQ(used_block2->GetPreviousBlock(), used_block1);
    ASSERT_EQ(used_block2->GetNextBlock(), mp.GetFirstFreeGenericBlock());

    ASSERT_NE(mp.GetFirstFreeGenericBlock(), nullptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetPreviousBlock(), used_block2);
}

TEST(MemoryDomainBlockPoolTest, DeallocateFirstSmallBlockAndNextGenericBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block0] = mp.AddFreeMemoryArea(mem_ptr, mem_size);
    auto first_size = first_block->GetBlockSize();

    auto min_data_size{sme::MemoryDomainUseBlock::GetMinimumDataSize()};
    auto min_used_block = mp.AllocateUseBlock(min_data_size);

    auto data_size2{mem_size / 2};
    auto used_block2 = mp.AllocateUseBlock(data_size2);

    mp.DeallocateUseBlock(min_used_block);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == first_block);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock() == used_block2);
    EXPECT_TRUE(used_block2->GetPreviousBlock() == mp.GetFirstFreeSmallBlock());

    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousFreeBlock() == nullptr);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == used_block2->GetNextBlock());
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == used_block2);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextFreeBlock() == nullptr);

    auto redzone_block = mp.GetFirstFreeGenericBlock()->GetNextBlock();

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(redzone_block == redzone_block0);
    ASSERT_TRUE(sme::IsRedZone(*redzone_block));
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == mp.GetFirstFreeGenericBlock());

    mp.DeallocateUseBlock(used_block2);

    EXPECT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == first_block);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetBlockSize(), first_size);

    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextFreeBlock() == nullptr);

    redzone_block = mp.GetFirstFreeGenericBlock()->GetNextBlock();

    ASSERT_TRUE(redzone_block != nullptr);
    ASSERT_TRUE(sme::IsRedZone(*redzone_block));
    ASSERT_TRUE(redzone_block->GetPreviousBlock() == mp.GetFirstFreeGenericBlock());
}

TEST(MemoryDomainBlockPoolTest, AllocateAndDeallocateFirstFourSmallBlocks)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block0] = mp.AddFreeMemoryArea(mem_ptr, mem_size);
    auto first_block_size = first_block->GetBlockSize();

    auto used_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_TRUE(used_block1 != nullptr);

    auto used_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_TRUE(used_block2 != nullptr);

    auto used_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_TRUE(used_block3 != nullptr);

    auto used_block4 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_TRUE(used_block4 != nullptr);

    mp.DeallocateUseBlock(used_block1);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == first_block);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock() == used_block2);
    EXPECT_TRUE(used_block2->GetPreviousBlock() == mp.GetFirstFreeSmallBlock());
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousFreeBlock() == nullptr);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == used_block4->GetNextBlock());
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == used_block4);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextFreeBlock() == nullptr);

    auto redzone_block = mp.GetFirstFreeGenericBlock()->GetNextBlock();
    ASSERT_EQ(redzone_block, redzone_block0);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == redzone_block);

    auto used_block31 = used_block3;
    mp.DeallocateUseBlock(used_block3);

    EXPECT_TRUE(mp.GetFirstFreeSmallBlock() == used_block31);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextFreeBlock() == first_block);
    EXPECT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextFreeBlock()->GetPreviousFreeBlock() ==
                mp.GetFirstFreeSmallBlock());

    mp.DeallocateUseBlock(used_block2);

    EXPECT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == first_block);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetBlockSize(),
              sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 3);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == used_block4);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                used_block4->GetNextBlock());
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock() ==
                mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock());

    mp.DeallocateUseBlock(used_block4);

    EXPECT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == first_block);
    EXPECT_EQ(mp.GetFirstFreeGenericBlock()->GetBlockSize(), first_block_size);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == redzone_block);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() == nullptr);
    EXPECT_TRUE(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest,
     AllocateAndDeallocateGenericBlocksInFirstAndLastLocationAndFourSmallBlocksInMiddle)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    auto [first_block, redzone_block] = mp.AddFreeMemoryArea(mem_ptr, mem_size);
    auto total_block_size = first_block->GetBlockSize();

    ASSERT_TRUE(sme::IsRedZone(*redzone_block));

    auto generic_data_size = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 6;
    auto generic_block = mp.AllocateUseBlock(generic_data_size);
    auto generic_block_size = generic_block->GetBlockSize();

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    auto small_block4 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto last_size =
        small_block4->GetBlockSize() + small_block4->GetNextBlock()->GetBlockSize();

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block4->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == redzone_block);
    ASSERT_FALSE(mp.GetFirstFreeGenericBlock()->IsFirstBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    mp.DeallocateUseBlock(generic_block);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block1->GetPreviousBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                small_block4->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->IsFirstBlock());

    mp.DeallocateUseBlock(small_block2);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == small_block3->GetPreviousBlock());

    mp.DeallocateUseBlock(small_block4);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == small_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock() == small_block3);
    ASSERT_TRUE(small_block3->GetPreviousBlock() == mp.GetFirstFreeSmallBlock());
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextFreeBlock() == nullptr);
    ASSERT_EQ(small_block3->GetNextBlock()->GetBlockSize(), last_size);

    mp.DeallocateUseBlock(small_block1);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == mem_ptr);
    ASSERT_EQ(
        mp.GetFirstFreeGenericBlock()->GetBlockSize(),
        generic_block_size + (sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 2));
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() ==
                mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock());

    mp.DeallocateUseBlock(small_block3);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == mem_ptr);
    ASSERT_EQ(mp.GetFirstFreeGenericBlock()->GetBlockSize(), total_block_size);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == redzone_block);
}

TEST(MemoryDomainBlockPoolTest, AllocateAndDeallocateGenericBlockInMiddle)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto gen_data_size1 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 5 + 1;
    (void)mp.AllocateUseBlock(gen_data_size1);

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size2 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3;
    auto gen_block2 = mp.AllocateUseBlock(gen_data_size2);

    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size3 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3 + 1;
    (void)mp.AllocateUseBlock(gen_data_size3);

    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    // ug1 us1 ug2 us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block3->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    mp.DeallocateUseBlock(gen_block2);

    // ug1 us1 fg us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block2->GetPreviousBlock());

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == small_block2);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == small_block1);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                small_block3->GetNextBlock());
    ASSERT_TRUE(
        mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock() ==
        mp.GetFirstFreeGenericBlock());
}

TEST(MemoryDomainBlockPoolTest,
     ReallocateGenericBlockForLessSizeInMiddleWithRestGenericBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto gen_data_size1 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 5 + 1;
    (void)mp.AllocateUseBlock(gen_data_size1);

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size2 = sme::MemoryDomainBlock::GetMinimumBlockSize() * 3 +
                          sme::MemoryDomainUseBlock::GetMinimumDataSize();
    auto gen_block2 = mp.AllocateUseBlock(gen_data_size2);

    ASSERT_EQ(gen_block2->GetBlockSize(),
              sme::MemoryDomainBlock::GetMinimumBlockSize() * 4);

    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size3 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3 + 1;
    (void)mp.AllocateUseBlock(gen_data_size3);

    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    // ug1 us1 ug2 us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block3->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    mp.DeallocateUseBlock(gen_block2);

    // ug1 us1 fg us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == small_block2);
    ASSERT_TRUE(small_block2->GetPreviousBlock() == mp.GetFirstFreeGenericBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                small_block3->GetNextBlock());
    ASSERT_TRUE(
        mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock() ==
        mp.GetFirstFreeGenericBlock());

    auto gen_data_size4 = gen_data_size2 / 2;
    auto gen_block4 = mp.AllocateUseBlock(gen_data_size4);

    // ug1 us1 ug4 fg us2 ug3 us3 fg

    ASSERT_EQ(gen_block4->GetBlockSize(),
              sme::MemoryDomainUseBlock::CalculateBlockSizeForData(gen_data_size4));

    ASSERT_TRUE(gen_block4 == small_block1->GetNextBlock());
    ASSERT_TRUE(gen_block4->GetPreviousBlock() == small_block1);

    ASSERT_TRUE(gen_block4->GetNextBlock() != nullptr);
    ASSERT_TRUE(gen_block4->GetNextBlock()->GetNextBlock() == small_block2);
    ASSERT_TRUE(gen_block4->GetNextBlock() == small_block2->GetPreviousBlock());
    ASSERT_TRUE(gen_block4->GetNextBlock()->GetPreviousBlock() == gen_block4);
    ASSERT_TRUE(gen_block4->GetNextBlock()->GetNextBlock() == small_block2);

    ASSERT_GT(gen_block4->GetNextBlock()->GetBlockSize(),
              sme::MemoryDomainBlock::GetMinimumBlockSize());

    ASSERT_EQ(gen_block4->GetNextBlock()->GetType(),
              sme::MemoryDomainBlock::Type::kFreeGeneric);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                small_block3->GetNextBlock());
    ASSERT_TRUE(
        mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock() ==
        mp.GetFirstFreeGenericBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == gen_block4);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest,
     ReallocateGenericBlockForLessSizeInMiddleWithRestSmallBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto gen_data_size1 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 5 + 1;
    (void)mp.AllocateUseBlock(gen_data_size1);

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size2 = sme::MemoryDomainBlock::GetMinimumBlockSize() * 2 +
                          sme::MemoryDomainUseBlock::GetMinimumDataSize();
    auto gen_block2 = mp.AllocateUseBlock(gen_data_size2);

    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size3 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3 + 1;
    (void)mp.AllocateUseBlock(gen_data_size3);

    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    // ug1 us1 ug2 us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block3->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    mp.DeallocateUseBlock(gen_block2);

    // ug1 us1 fg us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == small_block2);
    ASSERT_TRUE(small_block2->GetPreviousBlock() == mp.GetFirstFreeGenericBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                small_block3->GetNextBlock());
    ASSERT_TRUE(
        mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock() ==
        mp.GetFirstFreeGenericBlock());

    auto gen_data_size4 =
        gen_data_size2 - sme::MemoryDomainUseBlock::GetMinimumBlockSize();
    auto gen_block4 = mp.AllocateUseBlock(gen_data_size4);

    // ug1 us1 ug4 fs us2 ug3 us3 fg

    ASSERT_EQ(gen_block4->GetBlockSize(),
              sme::MemoryDomainUseBlock::CalculateBlockSizeForData(gen_data_size4));

    ASSERT_TRUE(gen_block4 == small_block1->GetNextBlock());
    ASSERT_TRUE(gen_block4->GetPreviousBlock() == small_block1);

    ASSERT_TRUE(gen_block4->GetNextBlock() != nullptr);
    ASSERT_TRUE(gen_block4->GetNextBlock()->GetNextBlock() == small_block2);
    ASSERT_TRUE(gen_block4->GetNextBlock() == small_block2->GetPreviousBlock());
    ASSERT_TRUE(gen_block4->GetNextBlock()->GetPreviousBlock() == gen_block4);
    ASSERT_TRUE(gen_block4->GetNextBlock()->GetNextBlock() == small_block2);

    ASSERT_EQ(gen_block4->GetNextBlock()->GetBlockSize(),
              sme::MemoryDomainBlock::GetMinimumBlockSize());

    ASSERT_EQ(gen_block4->GetNextBlock()->GetType(),
              sme::MemoryDomainBlock::Type::kFreeSmall);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == small_block3);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == gen_block4->GetNextBlock());
}

TEST(MemoryDomainBlockPoolTest, ReallocateGenericBlockForSameSizeInMiddle)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto gen_data_size1 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 5 + 1;
    (void)mp.AllocateUseBlock(gen_data_size1);

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size2 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3;
    auto gen_block2 = mp.AllocateUseBlock(gen_data_size2);

    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    auto gen_data_size3 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3 + 1;
    (void)mp.AllocateUseBlock(gen_data_size3);

    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    // ug1 us1 ug2 us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block3->GetNextBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    mp.DeallocateUseBlock(gen_block2);

    // ug1 us1 fg us2 ug3 us3 fg

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == small_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == small_block2);
    ASSERT_TRUE(small_block2->GetPreviousBlock() == mp.GetFirstFreeGenericBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                small_block3->GetNextBlock());
    ASSERT_TRUE(
        mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock() ==
        mp.GetFirstFreeGenericBlock());

    auto gen_data_size4 = sme::MemoryDomainUseBlock::GetMinimumDataSize() * 3;
    auto gen_block4 = mp.AllocateUseBlock(gen_data_size4);

    // ug1 us1 ug4 us2 ug3 us3 fg

    ASSERT_EQ(gen_block4->GetBlockSize(),
              sme::MemoryDomainUseBlock::CalculateBlockSizeForData(gen_data_size4));
    ASSERT_EQ(gen_block4->GetBlockSize(),
              sme::MemoryDomainUseBlock::CalculateBlockSizeForData(gen_data_size2));

    ASSERT_TRUE(gen_block4 == small_block1->GetNextBlock());
    ASSERT_TRUE(gen_block4 == small_block2->GetPreviousBlock());
    ASSERT_TRUE(gen_block4->GetPreviousBlock() == small_block1);
    ASSERT_TRUE(gen_block4->GetNextBlock() == small_block2);

    ASSERT_EQ(gen_block4->GetNextBlock()->GetType(), sme::MemoryDomainBlock::Type::kUsed);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetLessFreeBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == small_block3);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
}

TEST(MemoryDomainBlockPoolTest,
     AllocateThreeSequentalSmallBlocksInMiddleFromFreeGenericBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto gen_block1 = mp.AllocateUseBlock(mem_size / 4);

    auto gen_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 2 +
                            sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_EQ(gen_block2->GetBlockSize(),
              sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 3);

    auto gen_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 5);

    auto last_free_gen_block = mp.GetFirstFreeGenericBlock();

    mp.DeallocateUseBlock(gen_block2);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != last_free_gen_block);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == gen_block1->GetNextBlock());

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    ASSERT_TRUE(small_block1 == gen_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(small_block1->GetNextBlock() == mp.GetFirstFreeGenericBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == small_block1);

    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    ASSERT_TRUE(small_block2 == small_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == gen_block3->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == gen_block3);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() != nullptr);
    ASSERT_TRUE(small_block2->GetNextBlock() == mp.GetFirstFreeSmallBlock());
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousBlock() == small_block2);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock() == gen_block3);

    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    ASSERT_TRUE(small_block3 == small_block2->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() != nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == gen_block3->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == gen_block3);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(small_block3->GetNextBlock() == gen_block3);
}

TEST(MemoryDomainBlockPoolTest, DeallocateSequentalFourSmallBlocksInMiddle)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto mem_size = mem_realm.GetCapacity();
    (void)mp.AddFreeMemoryArea(mem_ptr, mem_size);

    auto gen_block1 = mp.AllocateUseBlock(mem_size / 4);

    auto gen_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 3 +
                            sme::MemoryDomainUseBlock::GetMinimumDataSize());
    ASSERT_EQ(gen_block2->GetBlockSize(),
              sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 4);

    auto gen_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumBlockSize() * 5);

    (void)mp.GetFirstFreeGenericBlock();

    mp.DeallocateUseBlock(gen_block2);

    auto small_block1 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    auto small_block2 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    auto small_block3 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());
    auto small_block4 =
        mp.AllocateUseBlock(sme::MemoryDomainUseBlock::GetMinimumDataSize());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    mp.DeallocateUseBlock(small_block1);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() != nullptr);
    ASSERT_TRUE(small_block2->GetPreviousBlock() == mp.GetFirstFreeSmallBlock());

    mp.DeallocateUseBlock(small_block3);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() != nullptr);
    ASSERT_TRUE(small_block2->GetNextBlock() == mp.GetFirstFreeSmallBlock());
    ASSERT_TRUE(small_block4->GetPreviousBlock() == mp.GetFirstFreeSmallBlock());
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousBlock() == small_block2);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextBlock() == small_block4);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetNextFreeBlock() ==
                small_block2->GetPreviousBlock());

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock()->GetPreviousFreeBlock() == nullptr);

    auto next_free_small_block = mp.GetFirstFreeSmallBlock()->GetNextFreeBlock();
    ASSERT_TRUE(next_free_small_block->GetNextFreeBlock() == nullptr);
    ASSERT_TRUE(next_free_small_block->GetPreviousFreeBlock() ==
                mp.GetFirstFreeSmallBlock());

    mp.DeallocateUseBlock(small_block2);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == gen_block1->GetNextBlock());

    mp.DeallocateUseBlock(small_block4);

    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == gen_block1->GetNextBlock());
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetNextBlock() == gen_block3);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetPreviousBlock() == gen_block1);
    ASSERT_TRUE(mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock() ==
                gen_block3->GetNextBlock());
    ASSERT_TRUE(
        mp.GetFirstFreeGenericBlock()->GetGreaterFreeBlock()->GetLessFreeBlock() ==
        mp.GetFirstFreeGenericBlock());
    ASSERT_TRUE(gen_block3->GetPreviousBlock() == mp.GetFirstFreeGenericBlock());
}

TEST(MemoryDomainBlockPoolTest, AllocateAllSpace)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto [first_free_block, redzone_block] =
        mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());
    auto total_block_size = first_free_block->GetBlockSize();

    auto max_data_size = sme::MemoryDomainUseBlock::GetDataCapacityAt(*first_free_block);
    auto max_block = mp.AllocateUseBlock(max_data_size);

    ASSERT_TRUE(max_block != nullptr);
    ASSERT_EQ(max_block->GetBlockSize(), total_block_size);

    ASSERT_TRUE(mp.GetFirstFreeGenericBlock() == nullptr);
    ASSERT_TRUE(mp.GetFirstFreeSmallBlock() == nullptr);

    auto empty_block = mp.AllocateUseBlock(1);
    ASSERT_TRUE(empty_block == nullptr);
}

TEST(MemoryDomainBlockPoolTest, TryToAllocateOverflowSizedBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    (void)mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());

    auto first_block = mp.AllocateUseBlock(mem_realm.GetCapacity());
    ASSERT_TRUE(first_block == nullptr);

    first_block = mp.AllocateUseBlock(mem_realm.GetCapacity() + 1);
    ASSERT_TRUE(first_block == nullptr);

    first_block = mp.AllocateUseBlock(mem_realm.GetCapacity() * 2);
    ASSERT_TRUE(first_block == nullptr);
}

TEST(MemoryDomainBlockPoolTest, TryToAllocateZeroBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    (void)mp.AddFreeMemoryArea(mem_ptr, 0);

    auto first_block = mp.AllocateUseBlock(0);
    ASSERT_TRUE(first_block == nullptr);
}

TEST(MemoryDomainBlockPoolTest, ShrinkMemoryOnOneFreeBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto [first_block, redzone_block] =
        mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());

    auto new_redzone_block = mp.ShrinkMemoryAreaFrom(redzone_block);
    ASSERT_TRUE(first_block == new_redzone_block);
}

TEST(MemoryDomainBlockPoolTest, ShrinkMemoryOnOneUseBlock)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto [first_block, redzone_block] =
        mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());

    auto max_data_size = sme::MemoryDomainUseBlock::GetDataCapacityAt(*first_block);
    auto use_block = mp.AllocateUseBlock(max_data_size);

    auto new_redzone_block = mp.ShrinkMemoryAreaFrom(redzone_block);

    auto next_block = use_block->GetNextBlock();
    ASSERT_TRUE(next_block == new_redzone_block);
}

TEST(MemoryDomainBlockPoolTest, ShrinkMemoryOnOneUseAndOneFreeBlocks)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto [first_block, redzone_block] =
        mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());

    auto use_block = mp.AllocateUseBlock(1);

    const auto& new_redzone_block = mp.ShrinkMemoryAreaFrom(redzone_block);

    auto next_block = use_block->GetNextBlock();
    ASSERT_TRUE(next_block == new_redzone_block);
}

TEST(MemoryDomainBlockPoolTest, ShrinkMemoryOnOneFreeAndOneUseBlocks)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto [first_block, redzone_block] =
        mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());

    auto use_block1 = mp.AllocateUseBlock(1);
    auto use_block2 = mp.AllocateUseBlock(1);
    auto free_block = use_block2->GetNextBlock();

    mp.DeallocateUseBlock(use_block1);

    ASSERT_TRUE(free_block->GetNextBlock() == redzone_block);

    auto new_redzone_block = mp.ShrinkMemoryAreaFrom(redzone_block);

    ASSERT_TRUE(new_redzone_block->GetPreviousBlock() == use_block2);

    auto next_block = use_block2->GetNextBlock();
    ASSERT_TRUE(next_block == new_redzone_block);
}

TEST(MemoryDomainBlockPoolTest, ShrinkMemoryAndTryAllocate)
{
    auto mem_realm = CreateTestMemory();
    sme::Pointer<char> mem_ptr = mem_realm.GetBaseAddress();

    sme::MemoryDomainFreeBlockPool mp;

    auto [first_block, redzone_block] =
        mp.AddFreeMemoryArea(mem_ptr, mem_realm.GetCapacity());

    (void)mp.AllocateUseBlock(1);

    (void)mp.ShrinkMemoryAreaFrom(redzone_block);

    auto none_block = mp.AllocateUseBlock(1);
    ASSERT_TRUE(none_block == nullptr);
}
