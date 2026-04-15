#include "sme/internal/mem_domain_segment.h"

#include <cassert>
#include <new>
#include <stdexcept>
#include <string>

namespace sme {

MemoryDomainSegment::MemoryDomainSegment(Size mem_size) : mem_size_{mem_size}
{
    bool size_valid = (mem_size_ >= GetMinimumSegmentSize());
    assert(size_valid);
    if (!size_valid)
        throw std::invalid_argument("Memory segment must be greater or equal " +
                                    std::to_string(GetMinimumSegmentSize()));
}

auto MemoryDomainSegment::CalculateSegmentSizeForData(Size data_size) noexcept -> Size
{
    if (data_size == 0)
        return 0;

    auto res = sizeof(MemoryDomainSegment) +
               MemoryDomainUseBlock::CalculateBlockSizeForData(data_size) +
               MemoryDomainRedZoneBlock::kBlockSize;

    return (res >= GetMinimumSegmentSize() ? res : 0);
}

auto MemoryDomainSegment::GetSegmentSize() const noexcept -> Size
{
    return mem_size_;
}

auto MemoryDomainSegment::GetDataCapacity() const noexcept -> Size
{
    return mem_size_ - sizeof(*this);
}

auto MemoryDomainSegment::GetData() const noexcept -> const char*
{
    return data_;
}

auto MemoryDomainSegment::GetData() noexcept -> char*
{
    return data_;
}

auto MemoryDomainSegment::GetBlockType(const Pointer<const void>& ptr) const noexcept
    -> MemoryDomainBlock::Type
{
    if (ptr == nullptr)
        return MemoryDomainBlock::Type::kInvalid;

    const char* addr = reinterpret_cast<const char*>(ptr.GetAddress());

    for (Pointer<MemoryDomainBlock> block = last_block_; block != nullptr;
         block = block->GetPreviousBlock()) {
        const char* block_addr = reinterpret_cast<const char*>(block.GetAddress());
        if (block_addr <= addr && addr < (block_addr + block->GetBlockSize()))
            return block->GetType();
    }

    return MemoryDomainBlock::Type::kInvalid;
}

auto MemoryDomainSegment::GetLastBlock() noexcept -> Pointer<MemoryDomainRedZoneBlock>
{
    return last_block_;
}

void MemoryDomainSegment::SetLastBlock(
    Pointer<MemoryDomainRedZoneBlock> last_block) noexcept
{
    assert(last_block != nullptr);
    assert(data_ <= last_block);
    assert((reinterpret_cast<const char*>(last_block.GetAddress()) +
            last_block->GetBlockSize()) <= (data_ + GetDataCapacity()));

    last_block_ = std::move(last_block);
}

void MemoryDomainSegment::ShrinkData(Pointer<MemoryDomainRedZoneBlock> last_block)
{
    SetLastBlock(last_block);

    mem_size_ = reinterpret_cast<const char*>(last_block.GetAddress()) +
                last_block_->GetBlockSize() - reinterpret_cast<const char*>(this);
}

auto MemoryDomainSegment::GetNextSegment() const noexcept
    -> Pointer<const MemoryDomainSegment>
{
    return next_segment_;
}

auto MemoryDomainSegment::GetNextSegment() noexcept -> Pointer<MemoryDomainSegment>
{
    return next_segment_;
}

void MemoryDomainSegment::SetNextSegment(Pointer<MemoryDomainSegment> seg) noexcept
{
    assert(next_segment_ == nullptr);
    if (next_segment_ != nullptr)
        return;

    next_segment_ = std::move(seg);
}

}  // namespace sme
