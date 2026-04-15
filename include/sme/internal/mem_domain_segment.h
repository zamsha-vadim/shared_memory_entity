#ifndef SME_MEM_DOMAIN_SEGMENT_H
#define SME_MEM_DOMAIN_SEGMENT_H

#include "sme/internal/mem_domain_block.h"
#include "sme/mem_ptr.h"

namespace sme {

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,-warnings-as-errors)

class MemoryDomainSegment {
   public:
    using Size = MemoryDomainBlock::Size;

   public:
    explicit MemoryDomainSegment(Size segment_size);
    ~MemoryDomainSegment() = default;

    MemoryDomainSegment(const MemoryDomainSegment&) = delete;
    MemoryDomainSegment(MemoryDomainSegment&&) = delete;
    auto operator=(const MemoryDomainSegment&) -> MemoryDomainSegment& = delete;
    auto operator=(MemoryDomainSegment&&) -> MemoryDomainSegment& = delete;

    auto GetSegmentSize() const noexcept -> Size;

    constexpr static auto GetMinimumSegmentSize() noexcept -> Size;
    static auto CalculateSegmentSizeForData(Size data_size) noexcept -> Size;

    auto GetDataCapacity() const noexcept -> Size;
    constexpr static auto GetMinimumDataCapacity() noexcept -> Size;

    auto GetData() const noexcept -> const char*;
    auto GetData() noexcept -> char*;

    auto GetBlockType(const Pointer<const void>&) const noexcept
        -> MemoryDomainBlock::Type;

    auto GetLastBlock() noexcept -> Pointer<MemoryDomainRedZoneBlock>;
    void SetLastBlock(Pointer<MemoryDomainRedZoneBlock> redzone_block) noexcept;

    void ShrinkData(Pointer<MemoryDomainRedZoneBlock> last_block);

    auto GetNextSegment() const noexcept -> Pointer<const MemoryDomainSegment>;
    auto GetNextSegment() noexcept -> Pointer<MemoryDomainSegment>;
    void SetNextSegment(Pointer<MemoryDomainSegment> seg) noexcept;

   private:
    Size mem_size_{0};
    Pointer<MemoryDomainRedZoneBlock> last_block_;
    Pointer<MemoryDomainSegment> next_segment_;

    alignas(kMemoryDomainBlockAlign) char data_[];
};

// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,-warnings-as-errors)

constexpr auto MemoryDomainSegment::GetMinimumSegmentSize() noexcept -> Size
{
    return (sizeof(MemoryDomainSegment) + GetMinimumDataCapacity());
}

constexpr auto MemoryDomainSegment::GetMinimumDataCapacity() noexcept -> Size
{
    return (MemoryDomainBlock::GetMinimumBlockSize() +
            MemoryDomainRedZoneBlock::kBlockSize);
}

}  // namespace sme

#endif  // SME_MEM_DOMAIN_SEGMENT_H
