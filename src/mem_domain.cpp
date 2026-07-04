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

constexpr uint64_t kMemoryDomainCheckTypeId{0x9F6680B020F62};

void ValidateMemorySpaceCapacity(const MemorySpace& mem_space)
{
    if (mem_space.GetCapacity() < MemoryDomain::kMinimumCapacity)
        throw std::invalid_argument("Memory space capacity must be greater or equal " +
                                    std::to_string(MemoryDomain::kMinimumCapacity) +
                                    ". Argument memory size is " +
                                    std::to_string(mem_space.GetCapacity()));
}

void ValidateSizes(size_t data_size, size_t mem_align)
{
    if (unlikely(data_size == 0))
        throw std::invalid_argument("Allocation size must be greater 0");
    if (unlikely(data_size >= std::numeric_limits<MemoryDomain::Size>::max()))
        throw std::invalid_argument(
            "Allocation size is greater " +
            std::to_string(std::numeric_limits<MemoryDomain::Size>::max()));

    if (unlikely(mem_align == 0 || mem_align >= std::numeric_limits<MemoryDomain::Size>::max()))
        throw std::invalid_argument("Invalid alignment value");
    if (unlikely(mem_align > 1 && (mem_align % 2) != 0))
        throw std::invalid_argument("Invalid alignment value: must be a power of 2");
}

void ReserveCapacity(MemoryDomain& mem_domain, size_t size)
{
    auto ptr = mem_domain.Allocate(size);
    if (unlikely(ptr == nullptr))
        throw std::bad_alloc();

    mem_domain.DisableAllocationExtensible();
    mem_domain.Deallocate(ptr);
}

auto ConstructMemoryDomainSegment(void* mem, MemoryDomainSegment::Size mem_size)
    -> Pointer<MemoryDomainSegment>
{
    assert(mem != nullptr);
    if (unlikely(mem == nullptr))
        throw std::invalid_argument("Segment memory location is null");

    return {new (mem) MemoryDomainSegment{mem_size}};
}

void DeleteMemoryDomainSegment(Pointer<MemoryDomainSegment>& segment) noexcept
{
    if (unlikely(segment == nullptr))
        return;

    segment->~MemoryDomainSegment();
    segment.Reset();
}

[[nodiscard]] auto LockSynchronizer(Pointer<Synchronizer>& sync)
    -> std::unique_lock<Synchronizer>
{
    return (sync != nullptr && sync->GetType() != SynchronizationType::kNone)
               ? std::unique_lock<Synchronizer>{*sync}
               : std::unique_lock<Synchronizer>{};
}

}  // namespace

MemoryDomain::MemoryDomain(MemorySpace& mem_space, SynchronizationType sync_type)
    : kTypeCheckValue{kMemoryDomainCheckTypeId},
      mem_space_{(ValidateMemorySpaceCapacity(mem_space), &mem_space)}
{
    if (sync_type != SynchronizationType::kNone)
        sync_ = Create<Synchronizer>(*this, sync_type);
}

MemoryDomain::MemoryDomain(MemorySpace& mem_space,
                           size_t domain_size,
                           SynchronizationType sync_type)
    : MemoryDomain(mem_space, sync_type)
{
    ReserveCapacity(*this, domain_size);
}

MemoryDomain::~MemoryDomain()
{
    ReleaseAllSegments();

    Delete<Synchronizer>(*this, sync_);
}

auto MemoryDomain::IsValidObjectId(const MemoryDomain& obj) noexcept -> bool
{
    return obj.kTypeCheckValue == kMemoryDomainCheckTypeId;
}

auto MemoryDomain::Allocate(size_t data_size, size_t mem_align) -> Pointer<void>
{
    ValidateSizes(data_size, mem_align);

    auto sync_guard = LockSynchronizer(sync_);

    auto block = AllocateBlock(data_size, mem_align);
    return (likely(block != nullptr)) ? Pointer<void>{block->GetData()} : Pointer<void>{};
}

void MemoryDomain::Deallocate(Pointer<void>& ptr) noexcept
{
    if (unlikely(ptr == nullptr))
        return;
    assert((reinterpret_cast<uintptr_t>(ptr.GetAddress()) %
            MemoryDomainUseBlock::kDataAlign) == 0);

    auto sync_guard = LockSynchronizer(sync_);

    Pointer<MemoryDomainUseBlock> block = ptr - sizeof(MemoryDomainUseBlock);
    free_block_pool_.DeallocateUseBlock(block);

    ptr.Reset();
}

void MemoryDomain::Deallocate(Pointer<void>&& ptr) noexcept
{
    Deallocate(ptr);
}

void MemoryDomain::DisableAllocationExtensible() noexcept
{
    auto sync_guard = LockSynchronizer(sync_);

    if (!alloc_extensible_)
        return;
    ShrinkSegments();
    alloc_extensible_ = false;
}

auto MemoryDomain::IsAllocationExtensible() const noexcept -> bool
{
    auto sync_guard = LockSynchronizer(sync_);
    return alloc_extensible_;
}

auto MemoryDomain::AllocateBlock(Size data_size, Size mem_align)
    -> Pointer<MemoryDomainUseBlock>
{
    assert(data_size != 0);

    auto block = free_block_pool_.AllocateUseBlock(data_size, mem_align);
    if (block != nullptr)
        return block;

    if (!alloc_extensible_)
        return {};

    if (unlikely(!AddFreeMemory(data_size)))
        return {};
    return free_block_pool_.AllocateUseBlock(data_size, mem_align);
}

auto MemoryDomain::GetMemorySpace() const noexcept -> MemorySpace&
{
    return *mem_space_;
}

auto MemoryDomain::AddFreeMemory(Size data_size) -> bool
{
    auto segment = AllocateSegment(data_size);
    if (unlikely(segment == nullptr))
        return false;

    auto [free_block, redzone_block] =
        free_block_pool_.AddFreeMemoryArea(segment->GetData(), segment->GetDataCapacity());
    if (unlikely(free_block == nullptr)) {
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
    if (unlikely(mem == nullptr))
        return {};

    return ConstructMemoryDomainSegment(mem, mem_size);
}

void MemoryDomain::DeallocateSegment(Pointer<MemoryDomainSegment>& segment) noexcept
{
    Pointer<void> mem = segment;

    DeleteMemoryDomainSegment(segment);
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

            [[maybe_unused]] auto res = mem_space_->Resize(segment, segment->GetSegmentSize());
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

auto MemoryDomain::GetAddressState(const Pointer<void>& ptr) const noexcept -> AddressState
{
    auto sync_guard = LockSynchronizer(sync_);

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
                break;
        }
    }

    return AddressState::kInvalid;
}

auto MemoryDomain::GetAllSegmentInfo() const -> std::deque<SegmentInfo>
{
    std::deque<SegmentInfo> seg_infos;

    for (Pointer<MemoryDomainSegment> segment = begin_segment_; segment != nullptr;
         segment = segment->GetNextSegment()) {
        SegmentInfo seg_info{};
        seg_info.address = segment.GetAddress();
        seg_info.size = segment->GetSegmentSize();

        seg_infos.emplace_back(seg_info);
    }

    return seg_infos;
}

// Public functions

auto CreateMemoryDomain(MemorySpace& mem_space, SynchronizationType sync_type)
    -> Pointer<MemoryDomain>
{
    return Create<MemoryDomain>(mem_space, mem_space, sync_type);
}

auto CreateMemoryDomain(MemorySpace& mem_space,
                        size_t domain_size,
                        SynchronizationType sync_type) -> Pointer<MemoryDomain>
{
    return Create<MemoryDomain>(mem_space, mem_space, domain_size, sync_type);
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
