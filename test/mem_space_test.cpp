#include <array>
#include <cassert>
#include <cstddef>
#include <deque>
#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

#include "sme/mem_space.h"

namespace {

constexpr size_t kSomeSpaceSize{1024UL * 128};
std::array<char, kSomeSpaceSize> some_memory{0};

auto CreateTestMemorySpace(size_t size = some_memory.size()) -> sme::MemorySpace
{
    assert(size <= some_memory.size());
    some_memory.fill(0);
    return sme::MemorySpace{sme::Pointer<void>{some_memory.data()}, size};
}

[[maybe_unused]] void PrintAllocations(const sme::MemorySpace& mem_space)
{
    for (const auto& alloc : mem_space) {
        std::cout << '[' << alloc.block << "-"
                  << static_cast<const void*>(static_cast<const char*>(alloc.block) +
                                              alloc.size)
                  << ", " << alloc.position << ", " << alloc.size << '('
                  << (alloc.size - sme::kMemorySpaceBlockHeaderSize) << "), "
                  << (alloc.free ? "free" : "used") << "]\n";
    }
    std::cout << std::endl;
}

}  // namespace

TEST(MemorySpaceTest, TestConstructor)
{
    sme::MemorySpace mem_space{sme::Pointer<void>{some_memory.data()},
                               some_memory.size()};

    bool is_aligned = (reinterpret_cast<uintptr_t>(some_memory.data()) %
                       sme::kMemorySpaceBlockAlign) == 0;

    if (is_aligned) {
        EXPECT_EQ(mem_space.GetBaseAddress(), some_memory.data());
    } else {
        EXPECT_GT(mem_space.GetBaseAddress(), some_memory.data());
    }
    EXPECT_LT(mem_space.GetCapacity(), some_memory.size());
}

TEST(MemorySpaceTest, AllocateEmpty)
{
    auto mem_space = CreateTestMemorySpace();

    EXPECT_THROW((void)mem_space.Allocate(0), std::invalid_argument);
}

TEST(MemorySpaceTest, AllocateOneByte)
{
    auto mem_space = CreateTestMemorySpace();

    auto ptr = mem_space.Allocate(4);
    ASSERT_TRUE(ptr != nullptr);

    ptr = mem_space.Allocate(1);
    ASSERT_TRUE(ptr != nullptr);
}

TEST(MemorySpaceTest, AllocateSomeBytes)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{123};

    EXPECT_TRUE(mem_space.Allocate(some_size) != nullptr);
    EXPECT_TRUE(mem_space.Allocate(some_size) != nullptr);
}

TEST(MemorySpaceTest, AllocateAllSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto max_data_size = mem_space.GetCapacity();
    EXPECT_TRUE(mem_space.Allocate(max_data_size) != nullptr);

    EXPECT_TRUE(mem_space.Allocate(1) == nullptr);
}

TEST(MemorySpaceTest, AllocateOverspace)
{
    auto mem_space = CreateTestMemorySpace();

    auto over_data_size = mem_space.GetCapacity() + 1;
    EXPECT_TRUE(mem_space.Allocate(over_data_size) == nullptr);
}

TEST(MemorySpaceTest, Allocate)
{
    size_t kSomeBlockSize{123};

    auto mem_space = CreateTestMemorySpace();

    auto ptr = mem_space.Allocate(kSomeBlockSize);

    ASSERT_TRUE(ptr != nullptr);

    auto alloc_info = mem_space.GetAllocationInfo(ptr);
    ASSERT_TRUE(alloc_info);
    EXPECT_FALSE(alloc_info->free);
}

TEST(MemorySpaceTest, AllocateUntilExhaused)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{123};

    while (mem_space.Allocate(some_size += some_size) != nullptr)
        some_size += 1;
}

TEST(MemorySpaceTest, AllocateAtLeastInvalid)
{
    auto mem_space = CreateTestMemorySpace();

    EXPECT_THROW((void)mem_space.AllocateAtLeast(0), std::invalid_argument);
}

TEST(MemorySpaceTest, AllocateAtLeastOneByte)
{
    auto mem_space = CreateTestMemorySpace();

    sme::Pointer<void> ptr;
    size_t res_size{};

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(1);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(res_size, mem_space.GetCapacity());

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(1);
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_EQ(res_size, 0);

    ptr = mem_space.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);
}

TEST(MemorySpaceTest, AllocateAsLeastSomeBytes)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{123};
    sme::Pointer<void> ptr;

    size_t res_size{};

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(some_size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(res_size, mem_space.GetCapacity());

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(1);
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_EQ(res_size, 0);

    ptr = mem_space.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);
}

TEST(MemorySpaceTest, AllocateAtLeastAllSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto max_data_size = mem_space.GetCapacity();

    sme::Pointer<void> ptr;
    size_t res_size{};

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(max_data_size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(res_size, mem_space.GetCapacity());

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(1);
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_EQ(res_size, 0);

    ptr = mem_space.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);
}

TEST(MemorySpaceTest, AllocateAtLeastOverspace)
{
    auto mem_space = CreateTestMemorySpace();

    auto over_data_size = mem_space.GetCapacity() + 1;

    sme::Pointer<void> ptr;
    size_t res_size{};

    std::tie(ptr, res_size) = mem_space.AllocateAtLeast(over_data_size);
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_EQ(res_size, 0);
}

TEST(MemorySpaceTest, ResizeInvalid)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{123};
    auto ptr = mem_space.Allocate(some_size);

    EXPECT_FALSE(mem_space.Resize(ptr, 0));

    sme::Pointer<void> null_ptr{};
    EXPECT_THROW((void)mem_space.Resize(null_ptr, some_size), std::invalid_argument);
}

TEST(MemorySpaceTest, TryResizeAllSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto max_data_size = mem_space.GetCapacity();
    auto ptr = mem_space.Allocate(max_data_size);

    ASSERT_TRUE(mem_space.Resize(ptr, max_data_size));
}

TEST(MemorySpaceTest, TryResizeOverSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto max_data_size = mem_space.GetCapacity();
    auto ptr = mem_space.Allocate(max_data_size);

    ASSERT_FALSE(mem_space.Resize(ptr, max_data_size + 1));
}

TEST(MemorySpaceTest, ExpandSomeSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{123};
    auto ptr = mem_space.Allocate(some_size);

    auto new_size1{some_size + 1};
    ASSERT_TRUE(mem_space.Resize(ptr, new_size1));

    auto new_size2{some_size * 10};
    ASSERT_TRUE(mem_space.Resize(ptr, new_size2));
}

TEST(MemorySpaceTest, ShrinkSomeSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{12345};
    auto ptr = mem_space.Allocate(some_size);

    auto new_size1{some_size / 2};
    ASSERT_TRUE(mem_space.Resize(ptr, new_size1));

    auto new_size2{1};
    ASSERT_TRUE(mem_space.Resize(ptr, new_size2));
    ASSERT_TRUE(mem_space.Resize(ptr, new_size2));
}

TEST(MemorySpaceTest, ShrinkVerySmall)
{
    auto mem_space = CreateTestMemorySpace();

    auto min_shrinkable_size{sme::MemorySpaceBlock::CalculateAllocationDataSize(1) + 1};
    auto ptr = mem_space.Allocate(min_shrinkable_size);

    auto some_space_size{123};
    auto next_ptr = mem_space.Allocate(some_space_size);
    ASSERT_TRUE(next_ptr != nullptr);

    auto non_shrinkable_size{min_shrinkable_size - 1};

    ASSERT_FALSE(mem_space.Resize(ptr, 0));
    ASSERT_FALSE(mem_space.Resize(ptr, 1));
    ASSERT_FALSE(mem_space.Resize(ptr, non_shrinkable_size));
    ASSERT_TRUE(mem_space.Resize(ptr, min_shrinkable_size));
    ASSERT_TRUE(mem_space.Resize(ptr, min_shrinkable_size + 1));
}

TEST(MemorySpaceTest, ShrinkAndExpandSomeSpace)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{1234};
    auto ptr1 = mem_space.Allocate(some_size);
    auto ptr2 = mem_space.Allocate(some_size);

    ASSERT_FALSE(mem_space.Resize(ptr1, some_size * 2));
    ASSERT_TRUE(mem_space.Resize(ptr2, some_size * 2));

    ASSERT_TRUE(mem_space.Resize(ptr1, some_size / 2));
    ASSERT_TRUE(mem_space.Resize(ptr1, some_size));
    ASSERT_FALSE(mem_space.Resize(ptr1, some_size + sme::kMemorySpaceBlockHeaderSize));
}

TEST(MemorySpaceTest, DeallocateNull)
{
    auto mem_space = CreateTestMemorySpace();
    sme::Pointer<void> ptr;

    ASSERT_NO_THROW(mem_space.Deallocate(ptr));
}

TEST(MemorySpaceTest, Deallocate)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{1234};
    auto ptr = mem_space.Allocate(some_size);
    auto dup_ptr = ptr;

    ASSERT_NO_THROW(mem_space.Deallocate(ptr));
    ASSERT_EQ(ptr, nullptr);

    auto alloc_info = mem_space.GetAllocationInfo(dup_ptr);
    ASSERT_TRUE(alloc_info);
    ASSERT_TRUE(alloc_info->free);

    ptr = mem_space.Allocate(some_size);
    ASSERT_NE(ptr, nullptr);
    dup_ptr = ptr;

    ASSERT_NO_THROW(mem_space.Deallocate(ptr));
    ASSERT_EQ(ptr, nullptr);

    alloc_info = mem_space.GetAllocationInfo(dup_ptr);
    if (alloc_info) {
        EXPECT_TRUE(alloc_info->free);
    }
}

TEST(MemorySpaceTest, DeallocateRvalue)
{
    auto mem_space = CreateTestMemorySpace();

    constexpr auto some_size{1234U};

    mem_space.Deallocate(mem_space.Allocate(some_size));
}

TEST(MemorySpaceTest, DeallocateAll)
{
    auto mem_space = CreateTestMemorySpace();

    std::deque<sme::Pointer<void>> alloc_ptrs;
    auto some_size{1U};

    while (auto ptr = mem_space.Allocate(some_size)) {
        alloc_ptrs.push_back(ptr);
        some_size += 10;
    }

    for (auto& ptr : alloc_ptrs)
        ASSERT_NO_THROW(mem_space.Deallocate(ptr));

    ASSERT_TRUE(mem_space.Allocate(100) != nullptr);
}

TEST(MemorySpaceTest, CombineAllocationDeallocation)
{
    auto mem_space = CreateTestMemorySpace();

    std::deque<sme::Pointer<void>> alloc_ptrs;

    auto some_size1{100};
    auto some_size2{10};
    auto some_size3{1};
    auto some_size4{100};
    auto some_size5{150};
    auto some_size6{1};

    (void)mem_space.Allocate(some_size1);
    auto ptr2 = mem_space.Allocate(some_size2);
    auto ptr3 = mem_space.Allocate(some_size3);
    auto ptr4 = mem_space.Allocate(some_size4);
    (void)mem_space.Allocate(some_size5);
    (void)mem_space.AllocateAtLeast(some_size6);

    mem_space.Deallocate(ptr2);
    mem_space.Deallocate(ptr3);
    mem_space.Deallocate(ptr4);

    auto ptr7 = mem_space.Allocate(some_size2 + some_size3 + some_size4);
    ASSERT_TRUE(ptr7 != nullptr);

    mem_space.Deallocate(ptr7);

    auto [ptr8, size8] = mem_space.AllocateAtLeast(1);
    ASSERT_TRUE(ptr8 != nullptr);
}

TEST(MemorySpaceTest, CombineAllocationDeallocationForBounds)
{
    auto mem_space = CreateTestMemorySpace();

    std::deque<sme::Pointer<void>> alloc_ptrs;

    auto some_size{1};

    auto ptr1 = mem_space.Allocate(some_size);
    auto [ptr2, size] = mem_space.AllocateAtLeast(some_size);

    mem_space.Deallocate(ptr1);

    ptr1 = mem_space.Allocate(some_size);
    ASSERT_TRUE(ptr1 != nullptr);

    mem_space.Deallocate(ptr1);
    mem_space.Deallocate(ptr2);

    auto ptr3 = mem_space.Allocate(mem_space.GetCapacity());
    ASSERT_TRUE(ptr3 != nullptr);
}

TEST(MemorySpaceTest, TestAllocationPositionAfterDeallocation)
{
    auto mem_space = CreateTestMemorySpace();

    std::deque<sme::Pointer<void>> alloc_ptrs;

    auto some_size1{100};
    auto some_size2{1};
    auto some_size3{20};
    auto some_size4{100};
    auto some_size5{150};
    auto some_size6{1};

    (void)mem_space.Allocate(some_size1);
    (void)mem_space.Allocate(some_size2);
    auto ptr3 = mem_space.Allocate(some_size3);
    auto dup_ptr3 = ptr3;
    auto ptr4 = mem_space.Allocate(some_size4);
    auto ptr5 = mem_space.Allocate(some_size5);
    auto [ptr6, size6] = mem_space.AllocateAtLeast(some_size6);

    mem_space.Deallocate(ptr3);
    mem_space.Deallocate(ptr4);
    mem_space.Deallocate(ptr5);

    auto ptr7 = mem_space.Allocate(1);

    ASSERT_TRUE(ptr7 != nullptr);
    ASSERT_EQ(dup_ptr3, ptr7);

    mem_space.Deallocate(ptr7);
    mem_space.Deallocate(ptr6);

    auto ptr8 = mem_space.Allocate(30);

    ASSERT_TRUE(ptr8 != nullptr);
    ASSERT_EQ(dup_ptr3, ptr8);
}

TEST(MemorySpaceTest, GetValidAllocationInfo)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{1000U};

    auto ptr1 = mem_space.Allocate(some_size);
    auto dup_ptr1 = ptr1;

    auto alloc_info = mem_space.GetAllocationInfo(ptr1);
    ASSERT_TRUE(alloc_info);
    ASSERT_FALSE(alloc_info->free);
    ASSERT_EQ(alloc_info->size, sme::MemorySpaceBlock::CalculateBlockSize(some_size));

    auto ptr2 = mem_space.Allocate(some_size);
    auto dup_ptr2 = ptr2;

    auto ptr3 = mem_space.Allocate(some_size);
    auto dup_ptr3 = ptr3;

    mem_space.Deallocate(ptr3);
    mem_space.Deallocate(ptr2);
    mem_space.Deallocate(ptr1);

    alloc_info = mem_space.GetAllocationInfo(dup_ptr3);
    if (alloc_info) {
        EXPECT_TRUE(alloc_info->free);
    }

    alloc_info = mem_space.GetAllocationInfo(dup_ptr2);
    if (alloc_info) {
        EXPECT_TRUE(alloc_info->free);
    }

    alloc_info = mem_space.GetAllocationInfo(dup_ptr1);
    ASSERT_TRUE(alloc_info);
    EXPECT_TRUE(alloc_info->free);
}

TEST(MemorySpaceTest, GetInvalidAllocationInfo)
{
    auto mem_space = CreateTestMemorySpace();

    auto alloc_info = mem_space.GetAllocationInfo({});
    ASSERT_FALSE(alloc_info);

    sme::Pointer<void> ptr{mem_space.GetBaseAddress()};
    ptr += mem_space.GetCapacity();

    alloc_info = mem_space.GetAllocationInfo(ptr);
    ASSERT_FALSE(alloc_info);
}

TEST(MemorySpaceTest, TestBeginIterator)
{
    auto mem_space = CreateTestMemorySpace();

    EXPECT_TRUE(mem_space.begin() != mem_space.end());
    EXPECT_TRUE(mem_space.end() != mem_space.begin());

    auto begin_iter = mem_space.begin();
    ASSERT_TRUE(begin_iter);
    ASSERT_FALSE(!begin_iter);
    ASSERT_TRUE(begin_iter.GetValue().free);
}

TEST(MemorySpaceTest, TestEndIterator)
{
    auto mem_space = CreateTestMemorySpace();

    EXPECT_TRUE(mem_space.begin() != mem_space.end());
    EXPECT_TRUE(mem_space.end() != mem_space.begin());

    auto end_iter = mem_space.end();
    ASSERT_FALSE(end_iter);
    ASSERT_TRUE(!end_iter);
    ASSERT_ANY_THROW((void)end_iter.GetValue());
}

TEST(MemorySpaceTest, TestPrefixIteration)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{100U};

    auto ptr1 = mem_space.Allocate(some_size);
    auto ptr2 = mem_space.Allocate(some_size);
    auto ptr3 = mem_space.Allocate(some_size);

    auto iter = mem_space.begin();
    ASSERT_TRUE(iter.GetValue().data == ptr1);

    auto chk_iter = ++iter;
    ASSERT_TRUE(chk_iter.GetValue().data == ptr2);
    ASSERT_TRUE(iter.GetValue().data == ptr2);

    chk_iter = ++iter;
    ASSERT_TRUE(chk_iter.GetValue().data == ptr3);
    ASSERT_TRUE(iter.GetValue().data == ptr3);

    chk_iter = ++iter;  // Last free block
    ASSERT_TRUE(chk_iter != mem_space.end());
    ASSERT_TRUE(chk_iter);
    ASSERT_FALSE(!chk_iter);
    ASSERT_TRUE(chk_iter.GetValue().free);

    ASSERT_TRUE(iter != mem_space.end());
    ASSERT_TRUE(iter);
    ASSERT_FALSE(!iter);

    chk_iter = ++iter;  // End
    ASSERT_TRUE(chk_iter == mem_space.end());
    ASSERT_FALSE(chk_iter);
    ASSERT_TRUE(!chk_iter);

    ASSERT_TRUE(iter == mem_space.end());
    ASSERT_FALSE(iter);
    ASSERT_TRUE(!iter);
}

TEST(MemorySpaceTest, TestPostfixIteration)
{
    auto mem_space = CreateTestMemorySpace();

    auto some_size{100U};

    auto ptr1 = mem_space.Allocate(some_size);
    auto ptr2 = mem_space.Allocate(some_size);
    auto ptr3 = mem_space.Allocate(some_size);

    auto iter = mem_space.begin();
    ASSERT_TRUE(iter.GetValue().data == ptr1);

    auto chk_iter = iter++;
    ASSERT_TRUE(chk_iter.GetValue().data == ptr1);
    ASSERT_TRUE(iter.GetValue().data == ptr2);

    chk_iter = iter++;
    ASSERT_TRUE(chk_iter.GetValue().data == ptr2);
    ASSERT_TRUE(iter.GetValue().data == ptr3);

    chk_iter = iter++;  // Last used block
    ASSERT_TRUE(chk_iter != mem_space.end());
    ASSERT_TRUE(chk_iter);
    ASSERT_FALSE(!chk_iter);
    ASSERT_TRUE(chk_iter.GetValue().data == ptr3);

    ASSERT_TRUE(iter != mem_space.end());
    ASSERT_TRUE(iter);
    ASSERT_FALSE(!iter);

    chk_iter = iter++;  // Last free block
    ASSERT_TRUE(chk_iter != mem_space.end());
    ASSERT_TRUE(chk_iter);
    ASSERT_FALSE(!chk_iter);

    ASSERT_TRUE(iter == mem_space.end());
    ASSERT_FALSE(iter);
    ASSERT_TRUE(!iter);

    chk_iter = iter++;  // End
    ASSERT_TRUE(chk_iter == mem_space.end());
    ASSERT_FALSE(chk_iter);
    ASSERT_TRUE(!chk_iter);

    ASSERT_TRUE(iter == mem_space.end());
    ASSERT_FALSE(iter);
    ASSERT_TRUE(!iter);
}
