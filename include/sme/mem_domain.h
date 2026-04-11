#ifndef SME_MEM_DOMAIN_H
#define SME_MEM_DOMAIN_H

#include <cstdint>

#include "sme/internal/mem_domain_block.h"
#include "sme/internal/mem_domain_segment.h"
#include "sme/mem_ptr.h"
#include "sme/mem_space.h"
#include "sme/mutex.h"
#include "sme/sme_export.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast,
// cppcoreguidelines-pro-type-reinterpret-cast,)

namespace sme {

class SME_EXPORT MemoryDomain {
   public:
    using Size = MemoryDomainBlock::Size;

    static constexpr Size kMinimumCapacity{MemoryDomainBlock::GetMinimumBlockSize() +
                                           MemoryDomainRedZoneBlock::kBlockSize};

    enum class AddressState : uint8_t { kInvalid, kFree, kUsed, kOther };

   public:
    explicit MemoryDomain(MemorySpace& mem_space,
                          Synchronizer::Type sync_type = Synchronizer::Type::kNone);

    MemoryDomain(const MemoryDomain&) = delete;
    MemoryDomain(MemoryDomain&&) = delete;
    auto operator=(const MemoryDomain&) -> MemoryDomain& = delete;
    auto operator=(MemoryDomain&&) -> MemoryDomain& = delete;

    ~MemoryDomain();

    [[nodiscard]] auto GetMemorySpace() const noexcept -> MemorySpace&;

    [[nodiscard]] auto Allocate(size_t data_size) -> Pointer<void>;
    void Deallocate(Pointer<void>& ptr) noexcept;
    void Deallocate(Pointer<void>&& ptr) noexcept;

    template <typename T>
    auto GetAddressState(const T* ptr) const noexcept -> AddressState
    {
        return GetAddressState(
            Pointer<void>{const_cast<void*>(reinterpret_cast<const void*>(ptr))});
    }

    auto GetAddressState(const Pointer<void>& ptr) const noexcept -> AddressState;

    [[nodiscard]] auto IsAllocationExtensible() const noexcept -> bool;
    void DisableAllocationExtensible() noexcept;

   private:
    [[nodiscard]] auto AllocateBlock(Size data_size) -> Pointer<MemoryDomainUseBlock>;
    auto AddFreeMemory(Size data_size) -> bool;

    [[nodiscard]] auto AllocateSegment(size_t data_size) -> Pointer<MemoryDomainSegment>;
    void DeallocateSegment(Pointer<MemoryDomainSegment>&) noexcept;
    void ShrinkSegments() noexcept;

    [[nodiscard]] auto GetBeginSegment() const noexcept
        -> Pointer<const MemoryDomainSegment>;
    void AddSegment(Pointer<MemoryDomainSegment>& segment) noexcept;
    void ReleaseAllSegments() noexcept;

   private:
    Pointer<MemorySpace> mem_space_;
    mutable Synchronizer sync_;
    Pointer<MemoryDomainSegment> begin_segment_;
    MemoryDomainFreeBlockPool free_block_pool_;
    bool alloc_extensible_{true};
};

[[nodiscard]] auto SME_EXPORT CreateMemoryDomain(
    MemorySpace& mem_space,
    Synchronizer::Type sync_type = Synchronizer::Type::kNone) -> Pointer<MemoryDomain>;

void SME_EXPORT DeleteMemoryDomain(Pointer<MemoryDomain>&) noexcept;
void SME_EXPORT DeleteMemoryDomain(Pointer<MemoryDomain>&&) noexcept;

}  // namespace sme

// NOLINTEND(cppcoreguidelines-pro-type-const-cast,
// cppcoreguidelines-pro-type-reinterpret-cast,)

#endif  // SME_MEM_DOMAIN_H
