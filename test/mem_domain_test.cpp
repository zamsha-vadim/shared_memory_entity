#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "sme/mem_domain.h"
#include "sme/mem_space.h"

namespace {

constexpr size_t kSomeSpaceSize{1024UL * 128};
std::array<char, kSomeSpaceSize> some_memory{};

[[maybe_unused]] auto CreateTestMemorySpace(size_t size = some_memory.size())
    -> sme::MemorySpace
{
    assert(size <= some_memory.size());
    some_memory.fill(0);
    return sme::MemorySpace{sme::Pointer<void>{some_memory.data()}, size};
}

auto CalculateSpaceSizeForMinimumDataCapacity() noexcept -> size_t
{
    auto capacity = sme::MemorySpaceBlock::CalculateBlockSize(sme::MemoryDomain::kMinimumCapacity);

    auto rest = reinterpret_cast<uintptr_t>(some_memory.data()) % sme::kMemorySpaceBlockAlign;
    if (rest != 0) 
        capacity += sme::kMemorySpaceBlockAlign - rest;
    
    return capacity;
}

}  // namespace

TEST(MemoryDomainTest, TestValidMinimumiSizeConstructor)
{
    auto mem_space = CreateTestMemorySpace(CalculateSpaceSizeForMinimumDataCapacity());

    EXPECT_NO_THROW(sme::MemoryDomain{mem_space});
}

TEST(MemoryDomainTest, TestInvalidSizeConstructor)
{
    auto mem_space =
        CreateTestMemorySpace(CalculateSpaceSizeForMinimumDataCapacity() - 1);

    EXPECT_THROW(sme::MemoryDomain{mem_space}, std::invalid_argument);
}

TEST(MemoryDomainTest, TestValidConstructor)
{
    auto mem_space =
        CreateTestMemorySpace(CalculateSpaceSizeForMinimumDataCapacity() + 1);

    EXPECT_NO_THROW(sme::MemoryDomain{mem_space});
}

TEST(MemoryDomainTest, TestValidConstructorForPredefinedSize)
{
    auto mem_space = CreateTestMemorySpace();
    auto some_valid_size = mem_space.GetCapacity() / 2;

    sme::MemoryDomain md{mem_space, some_valid_size};

    auto ptr = md.Allocate(some_valid_size);
    ASSERT_TRUE(ptr != nullptr);

    md.Deallocate(ptr);

    ptr = md.Allocate(some_valid_size);
    ASSERT_TRUE(ptr != nullptr);

    ASSERT_TRUE(md.Allocate(1) == nullptr);

    md.Deallocate(ptr);

    ptr = md.Allocate(some_valid_size / 2);
    ASSERT_TRUE(ptr != nullptr);

    auto ptr2 = md.Allocate(some_valid_size / 4);
    ASSERT_TRUE(ptr2 != nullptr);

    auto ptr3 = md.Allocate(some_valid_size / 5);
    ASSERT_TRUE(ptr3 != nullptr);
}

TEST(MemoryDomainTest, TestAllocateOneByte)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto ptr = md.Allocate(1);
    ASSERT_TRUE(ptr != nullptr);
}

TEST(MemoryDomainTest, TestAllocateSomeValidBytes)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto some_valid_size = mem_space.GetCapacity() / 2;

    auto ptr = md.Allocate(some_valid_size);
    ASSERT_TRUE(ptr != nullptr);
}

TEST(MemoryDomainTest, TestAllocateZeroBytes)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    ASSERT_THROW((void)md.Allocate(0), std::invalid_argument);
}

TEST(MemoryDomainTest, TestAllocateInvalidTooManyBytes)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto some_big_size = mem_space.GetCapacity();

    auto ptr = md.Allocate(some_big_size);
    ASSERT_TRUE(ptr == nullptr);
}

TEST(MemoryDomainTest, TestValidDeallocate)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto some_valid_size = mem_space.GetCapacity() / 2;

    auto ptr = md.Allocate(some_valid_size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE(md.GetAddressState(ptr) == sme::MemoryDomain::AddressState::kUsed);

    auto chk_ptr = ptr;

    ASSERT_NO_THROW(md.Deallocate(ptr));
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_TRUE(md.GetAddressState(chk_ptr) == sme::MemoryDomain::AddressState::kFree);
}

TEST(MemoryDomainTest, TestValidDeallocateRvalue)
{
    auto mem_space = CreateTestMemorySpace();
    sme::MemoryDomain md{mem_space};

    const auto some_valid_size = mem_space.GetCapacity() / 2;

    md.Deallocate(md.Allocate(some_valid_size));
}

TEST(MemoryDomainTest, TestNullPointerDeallocate)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    sme::Pointer<void> ptr{};

    ASSERT_NO_THROW(md.Deallocate(ptr));
}

/*
TEST(MemoryDomainTest, TestInvalidDeallocate)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto some_valid_size = mem_space.GetCapacity() / 2;

    auto ptr = md.Allocate(some_valid_size);
    ASSERT_TRUE(ptr != nullptr);

    sme::Pointer<int> ptr1 = ptr;
    sme::Pointer<void> ptr2 = ptr1 - 1;
    sme::Pointer<void> ptr3 = ptr1 + 1;

    ASSERT_THROW(md.Deallocate(ptr2), std::invalid_argument);
    ASSERT_THROW(md.Deallocate(ptr3), std::invalid_argument);
}
*/

TEST(MemoryDomainTest, TestAlignedAllocationWithAllAlignments)
{
    auto mem_space = CreateTestMemorySpace();

    const auto kDataSizeStep = 50UL;
    const auto kMaxDataSize = mem_space.GetCapacity() / 2;

    for (auto data_size = 1UL; data_size <= kMaxDataSize; data_size += kDataSizeStep) {
        sme::MemoryDomain mem_domain{mem_space};

        auto mem_space_pos =
            reinterpret_cast<uintptr_t>(mem_space.GetBaseAddress().GetAddress());

        for (size_t mem_align = 2; mem_align != 0; mem_align <<= 1) {
            auto dummy_ptr = mem_domain.Allocate(1);
            ASSERT_TRUE(dummy_ptr != nullptr);

            auto aligned_ptr = mem_domain.Allocate(data_size, mem_align);

            ASSERT_TRUE(aligned_ptr != nullptr)
                << "Data size: " << data_size << ", alignment: " << mem_align;
            auto data_pos = reinterpret_cast<uintptr_t>(aligned_ptr.GetAddress());
            ASSERT_TRUE((data_pos % mem_align) == 0);

            if ((data_pos - mem_space_pos + data_size) >= (mem_space.GetCapacity() / 2))
                break;
        }
    }
}

TEST(MemoryDomainTest, TestAlignedAllocationSequentially)
{
    auto mem_space = CreateTestMemorySpace();

    const size_t data_size{100};

    for (size_t mem_align = 2; mem_align <= 1024; mem_align <<= 1) {
        sme::MemoryDomain mem_domain{mem_space};

        auto dummy_ptr = mem_domain.Allocate(1);
        ASSERT_TRUE(dummy_ptr != nullptr);

        sme::Pointer<void> aligned_ptr;

        auto aligned_ptr1 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr1 != nullptr);

        auto aligned_ptr2 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr2 != nullptr);

        auto aligned_ptr3 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr3 != nullptr);

        aligned_ptr = aligned_ptr2;
        mem_domain.Deallocate(aligned_ptr2);
        ASSERT_TRUE(aligned_ptr2 == nullptr);

        aligned_ptr2 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr2 != nullptr);
        ASSERT_TRUE(aligned_ptr2 == aligned_ptr);

        aligned_ptr = aligned_ptr1;
        mem_domain.Deallocate(aligned_ptr1);
        ASSERT_TRUE(aligned_ptr1 == nullptr);

        aligned_ptr1 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr1 != nullptr);
        ASSERT_TRUE(aligned_ptr1 == aligned_ptr);

        aligned_ptr = aligned_ptr3;
        mem_domain.Deallocate(aligned_ptr3);
        ASSERT_TRUE(aligned_ptr3 == nullptr);

        aligned_ptr3 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr3 != nullptr);
        ASSERT_TRUE(aligned_ptr3 == aligned_ptr);
    }
}

TEST(MemoryDomainTest, TestAlignedAllocationSequentiallyInterleavedWithUnaligned)
{
    auto mem_space = CreateTestMemorySpace();

    const size_t data_size{100};

    for (size_t mem_align = 2; mem_align <= 1024; mem_align <<= 1) {
        sme::MemoryDomain mem_domain{mem_space};

        sme::Pointer<void> aligned_ptr;

        ASSERT_TRUE(mem_domain.Allocate(1) != nullptr);

        auto aligned_ptr1 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr1 != nullptr);

        ASSERT_TRUE(mem_domain.Allocate(1) != nullptr);

        auto aligned_ptr2 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr2 != nullptr);

        ASSERT_TRUE(mem_domain.Allocate(1) != nullptr);

        auto aligned_ptr3 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr3 != nullptr);

        aligned_ptr = aligned_ptr2;
        mem_domain.Deallocate(aligned_ptr2);
        ASSERT_TRUE(aligned_ptr2 == nullptr);

        aligned_ptr2 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr2 != nullptr);
        ASSERT_TRUE(aligned_ptr2 == aligned_ptr);

        aligned_ptr = aligned_ptr1;
        mem_domain.Deallocate(aligned_ptr1);
        ASSERT_TRUE(aligned_ptr1 == nullptr);

        aligned_ptr1 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr1 != nullptr);
        ASSERT_TRUE(aligned_ptr1 == aligned_ptr);

        aligned_ptr = aligned_ptr3;
        mem_domain.Deallocate(aligned_ptr3);
        ASSERT_TRUE(aligned_ptr3 == nullptr);

        aligned_ptr3 = mem_domain.Allocate(data_size, mem_align);
        ASSERT_TRUE(aligned_ptr3 != nullptr);
        ASSERT_TRUE(aligned_ptr3 == aligned_ptr);
    }
}

TEST(MemoryDomainTest, TestAddressState)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    EXPECT_TRUE(md.GetAddressState(sme::Pointer<void>{}) ==
                sme::MemoryDomain::AddressState::kInvalid);

    auto some_valid_size = mem_space.GetCapacity() / 2;

    auto ptr = md.Allocate(some_valid_size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE(md.GetAddressState(ptr) == sme::MemoryDomain::AddressState::kUsed);

    auto chk_ptr = ptr;

    ASSERT_NO_THROW(md.Deallocate(ptr));
    ASSERT_TRUE(ptr == nullptr);
    ASSERT_TRUE(md.GetAddressState(chk_ptr) == sme::MemoryDomain::AddressState::kFree);
}

TEST(MemoryDomainTest, TestAllocationOnExtensibleState)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    ASSERT_TRUE(md.IsAllocationExtensible());

    md.DisableAllocationExtensible();

    ASSERT_FALSE(md.IsAllocationExtensible());
}

TEST(MemoryDomainTest, TestDisableAllocationExtensibleForTwoSequentialAllocation)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto some_size = mem_space.GetCapacity() / 4;

    auto ptr1 = md.Allocate(some_size);
    ASSERT_TRUE(ptr1 != nullptr);

    ASSERT_TRUE(md.GetAddressState(ptr1) == sme::MemoryDomain::AddressState::kUsed);

    md.DisableAllocationExtensible();

    ASSERT_TRUE(md.GetAddressState(ptr1) == sme::MemoryDomain::AddressState::kUsed);

    auto ptr2 = md.Allocate(some_size);
    ASSERT_TRUE(ptr2 == nullptr);
}

TEST(MemoryDomainTest, TestDisableAllocationExtensibleForAllocationAndFreeAndAgainAllocation)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md{mem_space};

    auto some_size = mem_space.GetCapacity() / 4;

    auto ptr1 = md.Allocate(some_size);
    auto ptr2 = md.Allocate(some_size);

    ASSERT_TRUE(md.GetAddressState(ptr1) == sme::MemoryDomain::AddressState::kUsed);
    ASSERT_TRUE(md.GetAddressState(ptr2) == sme::MemoryDomain::AddressState::kUsed);

    auto del_ptr = ptr1;
    md.Deallocate(ptr1);

    ASSERT_TRUE(md.GetAddressState(del_ptr) == sme::MemoryDomain::AddressState::kFree);
    ASSERT_TRUE(md.GetAddressState(ptr2) == sme::MemoryDomain::AddressState::kUsed);

    md.DisableAllocationExtensible();

    ASSERT_TRUE(md.GetAddressState(del_ptr) == sme::MemoryDomain::AddressState::kFree);
    ASSERT_TRUE(md.GetAddressState(ptr2) == sme::MemoryDomain::AddressState::kUsed);

    auto ptr3 = md.Allocate(some_size);
    ASSERT_TRUE(ptr3 != nullptr);

    auto ptr4 = md.Allocate(1);
    ASSERT_TRUE(ptr4 == nullptr);
}

TEST(MemoryDomainTest, TestDisableAllocationExtensibleForSequentialDomains)
{
    auto mem_space = CreateTestMemorySpace();

    sme::MemoryDomain md1{mem_space};

    constexpr auto alloc_count{10u};
    std::vector<sme::Pointer<void>> ptrs1;

    for (auto i = 0u; i < alloc_count; i++) {
        auto ptr = md1.Allocate(1);
        ASSERT_TRUE(ptr != nullptr);

        ptrs1.push_back(ptr);
    }

    sme::MemoryDomain md2{mem_space};

    auto ptr = md2.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);

    md1.DisableAllocationExtensible();

    for (auto i = 0u; i < ptrs1.size(); i++) {
        ASSERT_TRUE(md1.GetAddressState(ptrs1[i]) ==
                    sme::MemoryDomain::AddressState::kUsed);
    }

    ptr = md1.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);

    std::vector<sme::Pointer<void>> ptrs2;

    for (auto i = 0u; i < alloc_count; i++) {
        auto ptr = md2.Allocate(1);
        ASSERT_TRUE(ptr != nullptr);

        ptrs2.push_back(ptr);
    }

    sme::MemoryDomain md3{mem_space};

    ptr = md3.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);

    md1.~MemoryDomain();

    for (auto i = 0u; i < alloc_count; i++) {
        auto ptr = md3.Allocate(1);
        ASSERT_TRUE(ptr != nullptr);
    }

    ptr = md1.Allocate(1);
    ASSERT_TRUE(ptr == nullptr);

    md2.DisableAllocationExtensible();

    ptr = md3.Allocate(1);
    ASSERT_TRUE(ptr != nullptr);

    do {
        ptr = md3.Allocate(1);
    } while (ptr != nullptr);

    md2.~MemoryDomain();

    ptr = md3.Allocate(1);
    ASSERT_TRUE(ptr != nullptr);
}
