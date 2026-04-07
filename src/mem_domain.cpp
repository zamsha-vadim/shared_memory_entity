#include "sme/mem_domain.h"

#include <cassert>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>

#include "sme/alloc_util.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-rvalue-reference-param-not-moved)

namespace sme {

namespace {

void ValidateMemorySpaceCapacity(const MemorySpace& mem_space)
{
    if (mem_space.GetCapacity() < MemoryDomain::kMinimumCapacity)
        throw std::invalid_argument("Memory space capacity must be greater or equal " +
                                    std::to_string(MemoryDomain::kMinimumCapacity) +
                                    ". Argument memory size is " +
                                    std::to_string(mem_space.GetCapacity()));
}

}  // namespace

MemoryDomain::MemoryDomain(MemorySpace& mem_space, Synchronizer::Type sync_type)
    : mem_space_{(ValidateMemorySpaceCapacity(mem_space), &mem_space)}, sync_{sync_type}
{
}

MemoryDomain::~MemoryDomain()
{
    ReleaseAllSegments();
}

auto MemoryDomain::Allocate(size_t data_size) -> Pointer<void>
{
    const std::lock_guard sync_guard{sync_};

    if (!alloc_extensible_)
        return {};

    if (data_size == 0)
        throw std::invalid_argument("Allocation size must be greater 0");

    auto block = AllocateBlock(data_size);

    return (block != nullptr) ? Pointer<void>{block->GetData()} : Pointer<void>{};
}

void MemoryDomain::Deallocate(Pointer<void>& ptr) noexcept
{
    if (ptr == nullptr)
        return;
    assert((reinterpret_cast<uintptr_t>(ptr.GetAddress()) %
            MemoryDomainUseBlock::kDataAlign) == 0);

    const std::lock_guard sync_guard{sync_};

    Pointer<MemoryDomainUseBlock> block = ptr - sizeof(MemoryDomainUseBlock);
    free_block_pool_.DeallocateUseBlock(block);

    ptr.Reset();
}

void MemoryDomain::Deallocate(Pointer<void>&& ptr) noexcept
{
    Deallocate(ptr);
}

auto MemoryDomain::GetAddressState(const Pointer<void>& ptr) const noexcept
    -> AddressState
{
    const std::lock_guard sync_guard{sync_};

    for (auto segment = begin_segment_; segment != nullptr;
         segment = segment->GetNextSegment()) {
        auto block_type = segment->GetBlockType(ptr);

        switch (block_type) {
            case MemoryDomainBlock::Type::kFreeSmall:
            case MemoryDomainBlock::Type::kFreeGeneric:
                return AddressState::kFree;

            case MemoryDomainBlock::Type::kUsed:
                return AddressState::kUsed;

            case MemoryDomainBlock::Type::kRedZone:
                return AddressState::kOther;

            default:
                continue;
        }
    }

    return AddressState::kInvalid;
}

void MemoryDomain::DisableAllocationExtensible() noexcept
{
    const std::lock_guard sync_guard{sync_};

    ShrinkSegments();

    alloc_extensible_ = false;
}

auto MemoryDomain::IsAllocationExtensible() const noexcept -> bool
{
    const std::lock_guard sync_guard{sync_};

    return alloc_extensible_;
}

auto MemoryDomain::AllocateBlock(Size data_size) -> Pointer<MemoryDomainUseBlock>
{
    assert(data_size != 0);

    auto block = free_block_pool_.AllocateUseBlock(data_size);
    if (block != nullptr)
        return block;

    if (!AddFreeMemory(data_size))
        return {};
    return free_block_pool_.AllocateUseBlock(data_size);
}

auto MemoryDomain::GetMemorySpace() const noexcept -> MemorySpace&
{
    return *mem_space_;
}

auto MemoryDomain::AddFreeMemory(Size data_size) -> bool
{
    auto segment = AllocateSegment(data_size);
    if (segment == nullptr)
        return false;

    auto [free_block, redzone_block] = free_block_pool_.AddFreeMemoryArea(
        segment->GetData(), segment->GetDataCapacity());
    if (free_block == nullptr) {
        DeallocateSegment(segment);
        return false;
    }

    assert(redzone_block != nullptr);
    assert(IsRedZone(*redzone_block));

    segment->SetLastBlock(redzone_block);

    AddSegment(segment);

    return true;
}

auto MemoryDomain::GetBeginSegment() const noexcept -> Pointer<const MemoryDomainSegment>
{
    return begin_segment_;
}

auto MemoryDomain::AllocateSegment(size_t data_size) -> Pointer<MemoryDomainSegment>
{
    auto [mem, mem_size] = mem_space_->AllocateAtLeast(
        MemoryDomainSegment::CalculateSegmentSizeForData(data_size));
    if (mem == nullptr)
        return {};

    return AllocateMemoryDomainSegment(mem, mem_size);
}

void MemoryDomain::DeallocateSegment(Pointer<MemoryDomainSegment>& segment) noexcept
{
    Pointer<void> mem = segment;

    DeallocateMemoryDomainSegment(segment);
    mem_space_->Deallocate(mem);
}

void MemoryDomain::ShrinkSegments() noexcept
{
    Pointer<MemoryDomainSegment> prev_segment{};
    auto segment = begin_segment_;

    while (segment != nullptr) {
        bool seg_empty{false};

        auto redzone_block = segment->GetLastBlock();
        if (redzone_block != nullptr) {
            auto new_redzone_block = free_block_pool_.ShrinkMemoryAreaFrom(redzone_block);

            segment->ShrinkData(new_redzone_block);

            [[maybe_unused]] auto res =
                mem_space_->Resize(segment, segment->GetSegmentSize());
            assert(res);

            seg_empty = (new_redzone_block == segment->GetData());
        } else {
            seg_empty = true;
        }

        if (seg_empty) {
            if (begin_segment_ == segment) {
                begin_segment_ = segment->GetNextSegment();

                DeallocateSegment(segment);

                segment = begin_segment_;
                prev_segment = nullptr;
            } else {
                auto next_segment = segment->GetNextSegment();
                if (prev_segment != nullptr)
                    prev_segment->SetNextSegment(next_segment);

                DeallocateSegment(segment);

                segment = next_segment;
            }
        } else {
            prev_segment = segment;
            segment = segment->GetNextSegment();
        }
    }
}

void MemoryDomain::AddSegment(Pointer<MemoryDomainSegment>& segment) noexcept
{
    assert(segment != nullptr);

    segment->SetNextSegment(begin_segment_);

    begin_segment_ = segment;
}

void MemoryDomain::ReleaseAllSegments() noexcept
{
    while (begin_segment_ != nullptr) {
        auto segment = begin_segment_;
        begin_segment_ = begin_segment_->GetNextSegment();

        DeallocateSegment(segment);
    }
}

auto CreateMemoryDomain(MemorySpace& mem_space, Synchronizer::Type sync_type)
    -> Pointer<MemoryDomain>
{
    return Create<MemoryDomain>(mem_space, mem_space, sync_type);
}


void DeleteMemoryDomain(Pointer<MemoryDomain>&& mem_domain) noexcept
{
    DeleteMemoryDomain(mem_domain);
}

void DeleteMemoryDomain(Pointer<MemoryDomain>& mem_domain) noexcept
{
    if (mem_domain == nullptr)
        return;

    Delete(mem_domain->GetMemorySpace(), mem_domain);
}

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast,
// cppcoreguidelines-rvalue-reference-param-not-moved)
