#ifndef SME_MEM_DOMAIN_BLOCK_H
#define SME_MEM_DOMAIN_BLOCK_H

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>

#include "sme/mem_ptr.h"

namespace sme {

constexpr auto kMemoryDomainBlockAlign{alignof(std::max_align_t)};

class alignas(kMemoryDomainBlockAlign) MemoryDomainBlock {
   public:
    using Size = uint32_t;
    using Offset = int32_t;

    enum class Type : uint8_t { kInvalid, kFreeSmall, kFreeGeneric, kUsed, kRedZone };

    constexpr static auto kBlockAlign{kMemoryDomainBlockAlign};

   public:
    MemoryDomainBlock(const MemoryDomainBlock&) = delete;
    MemoryDomainBlock(MemoryDomainBlock&&) = delete;
    auto operator=(const MemoryDomainBlock&) -> MemoryDomainBlock& = delete;
    auto operator=(MemoryDomainBlock&&) -> MemoryDomainBlock& = delete;

    auto GetType() const noexcept -> Type;

    auto GetBlockSize() const noexcept -> Size;
    static constexpr auto GetMinimumBlockSize() noexcept -> Size;

    auto GetPreviousBlock() const noexcept -> const Pointer<const MemoryDomainBlock>;
    auto GetPreviousBlock() noexcept -> Pointer<MemoryDomainBlock>;

    auto GetNextBlock() const noexcept -> const Pointer<const MemoryDomainBlock>;
    auto GetNextBlock() noexcept -> Pointer<MemoryDomainBlock>;

    auto GetPreviousBlockOffset() const noexcept -> Size;
    void SetPreviousBlockOffset(Size block_ofs) noexcept;

    auto IsFirstBlock() const noexcept -> bool;
    auto IsLastBlock() const noexcept -> bool;
    void SetAsLastBlock(bool state = true) noexcept;

   protected:
    MemoryDomainBlock(Type type,
                      Size block_size,
                      Size prev_block_ofs = 0,
                      bool is_last = true);

    ~MemoryDomainBlock();

   private:
    auto GetPreviousBlockAddress() const noexcept -> MemoryDomainBlock*;
    auto GetNextBlockAddress() const noexcept -> MemoryDomainBlock*;

   private:
    Type type_{Type::kInvalid};
    bool is_last_{true};

    Size block_size_{0};
    Size prev_block_ofs_{0};
};

class alignas(kMemoryDomainBlockAlign) MemoryDomainUseBlock : public MemoryDomainBlock {
   public:
    constexpr static auto kDataAlign{alignof(std::max_align_t)};

    constexpr static Size kDataSizeStep{kDataAlign > 16 ? kDataAlign : 16};

   public:
    explicit MemoryDomainUseBlock(Size block_size,
                                  Size prev_block_ofs = 0,
                                  bool is_last = false);

    auto GetData() const noexcept -> const char*;
    auto GetData() noexcept -> char*;

    auto GetDataSize() const noexcept -> Size;
    static auto GetDataCapacityAt(const MemoryDomainBlock& block) noexcept -> Size;

    auto HasAddress(const char*) const noexcept -> bool;

    static constexpr auto GetMinimumDataSize() noexcept -> Size;
    static constexpr auto GetMaximumDataSize() noexcept -> Size;

    static auto CalculateBlockSizeForData(Size data_size) noexcept -> Size;

   private:
    alignas(kDataAlign) char data_[0];  // NOLINT
};

class alignas(kMemoryDomainBlockAlign) MemoryDomainFreeBlock : public MemoryDomainBlock {
   public:
    MemoryDomainFreeBlock(const MemoryDomainFreeBlock&) = delete;
    MemoryDomainFreeBlock(MemoryDomainFreeBlock&&) = delete;
    auto operator=(const MemoryDomainFreeBlock&) -> MemoryDomainFreeBlock& = delete;
    auto operator=(MemoryDomainFreeBlock&&) -> MemoryDomainFreeBlock& = delete;

    auto GetPreviousFreeBlock() const noexcept -> Pointer<MemoryDomainFreeBlock>;
    void SetPreviousFreeBlock(Pointer<MemoryDomainFreeBlock> prev_block) noexcept;

    auto GetNextFreeBlock() const noexcept -> Pointer<MemoryDomainFreeBlock>;
    void SetNextFreeBlock(Pointer<MemoryDomainFreeBlock> next_block) noexcept;

    void UnlinkFreeBlockRelations() noexcept;

    auto CanBeSplittedForUse(MemoryDomainBlock::Size used_block_size) const noexcept
        -> bool;

   protected:
    using MemoryDomainBlock::MemoryDomainBlock;
    ~MemoryDomainFreeBlock();

    static auto CanBeSplittedForUse(MemoryDomainBlock::Size free_block_size,
                                    MemoryDomainBlock::Size used_block_size) noexcept
        -> bool;

   private:
    Pointer<MemoryDomainFreeBlock> free_prev_block_;
    Pointer<MemoryDomainFreeBlock> free_next_block_;
};

class alignas(kMemoryDomainBlockAlign) MemoryDomainFreeSmallBlock
    : public MemoryDomainFreeBlock {
   public:
    explicit MemoryDomainFreeSmallBlock(Size prev_block_ofs = 0, bool is_last = false)
        : MemoryDomainFreeBlock{Type::kFreeSmall,
                                MemoryDomainBlock::GetMinimumBlockSize(), prev_block_ofs,
                                is_last}
    {
    }
};

class alignas(kMemoryDomainBlockAlign) MemoryDomainFreeGenericBlock
    : public MemoryDomainFreeBlock {
   public:
    explicit MemoryDomainFreeGenericBlock(Size block_size,
                                          Size prev_block_ofs = 0,
                                          bool is_last = false);

    auto GetLessFreeBlock() const noexcept
        -> const Pointer<const MemoryDomainFreeGenericBlock>;
    auto GetLessFreeBlock() noexcept -> Pointer<MemoryDomainFreeGenericBlock>;
    void SetLessFreeBlock(Pointer<MemoryDomainFreeGenericBlock> less_block) noexcept;

    auto GetGreaterFreeBlock() const noexcept
        -> Pointer<const MemoryDomainFreeGenericBlock>;
    auto GetGreaterFreeBlock() noexcept -> Pointer<MemoryDomainFreeGenericBlock>;
    void SetGreaterFreeBlock(
        Pointer<MemoryDomainFreeGenericBlock> greater_block) noexcept;

   private:
    Pointer<MemoryDomainFreeGenericBlock> free_less_block_{};
    Pointer<MemoryDomainFreeGenericBlock> free_greater_block_{};
};

constexpr auto MemoryDomainBlock::GetMinimumBlockSize() noexcept -> Size
{
    return sizeof(MemoryDomainUseBlock) + MemoryDomainUseBlock::GetMinimumDataSize();
}

constexpr auto MemoryDomainUseBlock::GetMinimumDataSize() noexcept -> Size
{
    return kDataAlign;
}

constexpr auto MemoryDomainUseBlock::GetMaximumDataSize() noexcept
    -> MemoryDomainBlock::Size
{
    return (((std::numeric_limits<Size>::max() / kMemoryDomainBlockAlign) *
             kMemoryDomainBlockAlign) -
            (sizeof(MemoryDomainUseBlock) + GetMinimumDataSize()));
}

class alignas(kMemoryDomainBlockAlign) MemoryDomainRedZoneBlock
    : public MemoryDomainBlock {
   public:
    static constexpr auto kBlockSize{MemoryDomainBlock::GetMinimumBlockSize()};
    static constexpr auto kDataSize{kBlockSize - sizeof(MemoryDomainBlock)};

    using DataType = std::array<char, kDataSize>;

   public:
    MemoryDomainRedZoneBlock(Size prev_block_ofs);

    auto GetData() const noexcept -> const DataType&;

   private:
    DataType data_;
};

auto IsFree(const MemoryDomainBlock& block) noexcept -> bool;
auto IsUsed(const MemoryDomainBlock& block) noexcept -> bool;
auto IsRedZone(const MemoryDomainBlock& block) noexcept -> bool;

class MemoryDomainFreeBlockPool {
   public:
    MemoryDomainFreeBlockPool() = default;

    MemoryDomainFreeBlockPool(const MemoryDomainFreeBlockPool&) = delete;
    MemoryDomainFreeBlockPool(MemoryDomainFreeBlockPool&&) = delete;
    auto operator=(const MemoryDomainFreeBlockPool&)
        -> MemoryDomainFreeBlockPool& = delete;
    auto operator=(MemoryDomainFreeBlockPool&&) -> MemoryDomainFreeBlockPool& = delete;

    ~MemoryDomainFreeBlockPool() = default;

    [[nodiscard]] auto AddFreeMemoryArea(void* mem,
                                         MemoryDomainBlock::Size mem_size) noexcept
        -> std::pair<Pointer<MemoryDomainFreeBlock>, Pointer<MemoryDomainRedZoneBlock>>;

    [[nodiscard]] auto ShrinkMemoryAreaFrom(Pointer<MemoryDomainRedZoneBlock>&) noexcept
        -> Pointer<MemoryDomainRedZoneBlock>;

    [[nodiscard]] auto AllocateUseBlock(MemoryDomainBlock::Size data_size) noexcept
        -> Pointer<MemoryDomainUseBlock>;
    void DeallocateUseBlock(Pointer<MemoryDomainUseBlock>& used_block) noexcept;

    auto GetFirstFreeSmallBlock() const noexcept -> Pointer<MemoryDomainFreeSmallBlock>;
    auto GetFirstFreeGenericBlock() const noexcept
        -> Pointer<MemoryDomainFreeGenericBlock>;

    static auto CalculateInitialFreeBlockSize(MemoryDomainBlock::Size mem_size) noexcept
        -> MemoryDomainBlock::Size;

   private:
    [[nodiscard]] auto AllocateUseBlockFromSuitableSmallBlock() noexcept
        -> Pointer<MemoryDomainUseBlock>;

    [[nodiscard]] auto AllocateUseBlockFromSuitableGenericBlock(
        MemoryDomainBlock::Size block_size) noexcept -> Pointer<MemoryDomainUseBlock>;

    [[nodiscard]] auto AllocateUseBlockFromGenericBlock(
        MemoryDomainFreeGenericBlock& src_block,
        MemoryDomainBlock::Size block_size) noexcept -> Pointer<MemoryDomainUseBlock>;

    [[nodiscard]] auto AllocateFreeBlock(void* mem,
                                         MemoryDomainBlock::Size block_size,
                                         MemoryDomainBlock::Size prev_block_ofs) noexcept
        -> Pointer<MemoryDomainFreeBlock>;

    [[nodiscard]] auto AllocateFreeSmallBlock(
        void* mem,
        MemoryDomainBlock::Size prev_block_ofs) noexcept
        -> Pointer<MemoryDomainFreeSmallBlock>;

    [[nodiscard]] auto AllocateFreeGenericBlock(
        void* mem,
        MemoryDomainBlock::Size block_size,
        MemoryDomainBlock::Size prev_block_ofs) noexcept
        -> Pointer<MemoryDomainFreeGenericBlock>;

    void DestroyBlock(MemoryDomainBlock& block) noexcept;
    void DestroyBlock(MemoryDomainFreeSmallBlock& block) noexcept;
    void DestroyBlock(MemoryDomainFreeGenericBlock& block) noexcept;
    void DestroyBlock(MemoryDomainUseBlock& block) noexcept;

    [[nodiscard]] auto FindSuitableFreeGenericBlock(
        MemoryDomainBlock::Size block_size) noexcept
        -> Pointer<MemoryDomainFreeGenericBlock>;

    void LinkFreeSmallBlock(MemoryDomainFreeSmallBlock& new_block) noexcept;
    void UnlinkFreeSmallBlock(MemoryDomainFreeSmallBlock& block) noexcept;

    void LinkFreeGenericBlock(MemoryDomainFreeGenericBlock& new_block) noexcept;
    void AddEqualFreeGenericBlock(MemoryDomainFreeGenericBlock& src_block,
                                  MemoryDomainFreeGenericBlock& new_block) noexcept;
    void InsertAfterFreeGenericBlock(MemoryDomainFreeGenericBlock& src_block,
                                     MemoryDomainFreeGenericBlock& new_block) noexcept;
    void InsertBeforeFreeGenericBlock(MemoryDomainFreeGenericBlock& src_block,
                                      MemoryDomainFreeGenericBlock& new_block) noexcept;
    void UnlinkFreeGenericBlock(MemoryDomainFreeGenericBlock& block) noexcept;

   private:
    Pointer<MemoryDomainFreeSmallBlock> first_small_block_{};
    Pointer<MemoryDomainFreeGenericBlock> first_gen_block_{};
};

}  // namespace sme

#endif  // SME_MEM_DOMAIN_BLOCK_H
