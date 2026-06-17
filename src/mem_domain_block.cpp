#include "sme/internal/mem_domain_block.h"

#include <cassert>
#include <limits>

namespace sme {

//  MemoryDomainBlock class

MemoryDomainBlock::MemoryDomainBlock(Type type,
                                     Size block_size,
                                     Size prev_block_ofs,
                                     bool is_last)
    : type_{type}, is_last_{is_last}, block_size_{block_size}, prev_block_ofs_{prev_block_ofs}
{
    assert(GetBlockSize() >= GetMinimumBlockSize());
    assert((GetBlockSize() % kBlockAlign) == 0);
    assert((GetPreviousBlockOffset() % kBlockAlign) == 0);
}

MemoryDomainBlock::~MemoryDomainBlock()
{
    type_ = Type::kInvalid;
}

auto MemoryDomainBlock::GetType() const noexcept -> Type
{
    return type_;
}

auto MemoryDomainBlock::GetBlockSize() const noexcept -> Size
{
    return block_size_;
}

auto MemoryDomainBlock::GetPreviousBlock() const noexcept
    -> const Pointer<const MemoryDomainBlock>
{
    return GetPreviousBlockAddress();
}

auto MemoryDomainBlock::GetPreviousBlock() noexcept -> Pointer<MemoryDomainBlock>
{
    return GetPreviousBlockAddress();
}

auto MemoryDomainBlock::GetNextBlock() const noexcept -> const Pointer<const MemoryDomainBlock>
{
    return GetNextBlockAddress();
}

auto MemoryDomainBlock::GetNextBlock() noexcept -> Pointer<MemoryDomainBlock>
{
    return GetNextBlockAddress();
}

auto MemoryDomainBlock::GetPreviousBlockOffset() const noexcept -> Size
{
    return prev_block_ofs_;
}

void MemoryDomainBlock::SetPreviousBlockOffset(Size block_ofs) noexcept
{
    prev_block_ofs_ = block_ofs;
}

auto MemoryDomainBlock::IsFirstBlock() const noexcept -> bool
{
    return (prev_block_ofs_ == 0);
}

auto MemoryDomainBlock::IsLastBlock() const noexcept -> bool
{
    return is_last_;
}

void MemoryDomainBlock::SetAsLastBlock(bool state) noexcept
{
    is_last_ = state;
}

auto MemoryDomainBlock::GetPreviousBlockAddress() const noexcept -> MemoryDomainBlock*
{
    if (prev_block_ofs_ == 0)
        return nullptr;

    auto* prev_block_addr =
        const_cast<char*>(reinterpret_cast<const char*>(this) - prev_block_ofs_);
    return std::launder(reinterpret_cast<MemoryDomainBlock*>(prev_block_addr));
}

auto MemoryDomainBlock::GetNextBlockAddress() const noexcept -> MemoryDomainBlock*
{
    if (is_last_)
        return nullptr;

    auto* next_block_addr =
        const_cast<char*>(reinterpret_cast<const char*>(this) + block_size_);
    return std::launder(reinterpret_cast<MemoryDomainBlock*>(next_block_addr));
}

auto IsFree(const MemoryDomainBlock& block) noexcept -> bool
{
    return (block.GetType() == MemoryDomainBlock::Type::kFreeSmall ||
            block.GetType() == MemoryDomainBlock::Type::kFreeGeneric);
}

auto IsUsed(const MemoryDomainBlock& block) noexcept -> bool
{
    return (block.GetType() == MemoryDomainBlock::Type::kUsed);
}

auto IsRedZone(const MemoryDomainBlock& block) noexcept -> bool
{
    return (block.GetType() == MemoryDomainBlock::Type::kRedZone);
}

// MemoryDomainUseBlock class

MemoryDomainUseBlock::MemoryDomainUseBlock(Size block_size, Size prev_block_ofs, bool is_last)
    : MemoryDomainBlock{Type::kUsed, block_size, prev_block_ofs, is_last}
{
}

auto MemoryDomainUseBlock::GetData() const noexcept -> const char*
{
    return data_;
}

auto MemoryDomainUseBlock::GetData() noexcept -> char*
{
    return data_;
}

auto MemoryDomainUseBlock::GetDataSize() const noexcept -> Size
{
    return GetDataCapacityAt(*this);
}

auto MemoryDomainUseBlock::GetDataCapacityAt(const MemoryDomainBlock& block) noexcept -> Size
{
    return block.GetBlockSize() - sizeof(MemoryDomainUseBlock);
}

auto MemoryDomainUseBlock::HasAddress(const char* addr) const noexcept -> bool
{
    return (data_ <= addr && (addr < (data_ + GetDataSize())));
}

auto MemoryDomainUseBlock::CalculateBlockSizeForData(Size data_size) noexcept -> Size
{
    if (data_size == 0 || data_size > GetMaximumDataSize())
        return 0;

    auto block_size = sizeof(MemoryDomainUseBlock) + data_size;
    auto block_size_rest = block_size % kMemoryDomainBlockAlign;
    if (block_size_rest != 0)
        block_size += kMemoryDomainBlockAlign - block_size_rest;

    return block_size;
}

// MemoryDomainFreeBlock class

MemoryDomainFreeBlock::~MemoryDomainFreeBlock()
{
    UnlinkFreeBlockRelations();
}

auto MemoryDomainFreeBlock::GetPreviousFreeBlock() const noexcept
    -> Pointer<MemoryDomainFreeBlock>
{
    return free_prev_block_;
}

void MemoryDomainFreeBlock::SetPreviousFreeBlock(
    Pointer<MemoryDomainFreeBlock> prev_block) noexcept
{
    free_prev_block_ = std::move(prev_block);
}

auto MemoryDomainFreeBlock::GetNextFreeBlock() const noexcept -> Pointer<MemoryDomainFreeBlock>
{
    return free_next_block_;
}

void MemoryDomainFreeBlock::SetNextFreeBlock(
    Pointer<MemoryDomainFreeBlock> next_block) noexcept
{
    free_next_block_ = std::move(next_block);
}

void MemoryDomainFreeBlock::UnlinkFreeBlockRelations() noexcept
{
    if (free_prev_block_ != nullptr)
        free_prev_block_->SetNextFreeBlock(GetNextFreeBlock());

    if (free_next_block_ != nullptr)
        free_next_block_->SetPreviousFreeBlock(GetPreviousFreeBlock());

    free_prev_block_.Reset();
    free_next_block_.Reset();
}

auto MemoryDomainFreeBlock::CanBeSplittedForUseBlock(Size block_size,
                                                     Size mem_align) const noexcept
    -> std::pair<bool, Position>
{
    Size free_block_size = GetBlockSize();

    if (block_size > free_block_size)
        return {false, 0};

    if (mem_align > MemoryDomainUseBlock::kDataAlign) {
        auto free_block_begin_pos = reinterpret_cast<uintptr_t>(this);
        auto free_block_end_pos = free_block_begin_pos + free_block_size;

        auto block_data_pos = free_block_begin_pos + sizeof(MemoryDomainUseBlock);

        auto align_remainder = block_data_pos % mem_align;
        if (align_remainder != 0) {
            block_data_pos += mem_align - align_remainder;

            auto block_begin_pos = block_data_pos - sizeof(MemoryDomainUseBlock);
            while ((block_begin_pos - free_block_begin_pos) <
                   MemoryDomainBlock::GetMinimumBlockSize()) {
                block_begin_pos += mem_align;
            }

            auto block_end_pos = block_begin_pos + block_size;

            assert((block_begin_pos % kMemoryDomainBlockAlign) == 0);
            assert((block_end_pos % kMemoryDomainBlockAlign) == 0);

            if (block_end_pos > free_block_end_pos)
                return {false, 0};

            auto block_ofs = block_begin_pos - free_block_begin_pos;
            assert((block_ofs % kMemoryDomainBlockAlign) == 0);
            assert(block_ofs >= MemoryDomainBlock::GetMinimumBlockSize());

            return {true, block_ofs};
        }
    }

    auto res = CanBeSplittedForUseBlock(block_size);
    return {res, 0};
}

auto MemoryDomainFreeBlock::CanBeSplittedForUseBlock(Size block_size) const noexcept -> bool
{
    Size free_block_size = GetBlockSize();

    return block_size >= MemoryDomainBlock::GetMinimumBlockSize() &&
           (free_block_size == block_size ||
            (free_block_size > block_size &&
             (free_block_size - block_size) >= MemoryDomainBlock::GetMinimumBlockSize()));
}

// MemoryDomainRedZoneBlock class

MemoryDomainRedZoneBlock::MemoryDomainRedZoneBlock(Size prev_block_ofs)
    : MemoryDomainBlock{Type::kRedZone, MemoryDomainBlock::GetMinimumBlockSize(),
                        prev_block_ofs, true}
{
}

auto MemoryDomainRedZoneBlock::GetData() const noexcept -> const DataType&
{
    return data_;
}

// MemoryDomainFreeGenericBlock class

MemoryDomainFreeGenericBlock::MemoryDomainFreeGenericBlock(Size block_size,
                                                           Size prev_block_ofs,
                                                           bool is_last)
    : MemoryDomainFreeBlock{Type::kFreeGeneric, block_size, prev_block_ofs, is_last}
{
    assert(block_size >= MemoryDomainBlock::GetMinimumBlockSize());
}

auto MemoryDomainFreeGenericBlock::GetLessFreeBlock() const noexcept
    -> const Pointer<const MemoryDomainFreeGenericBlock>
{
    return free_less_block_;
}

auto MemoryDomainFreeGenericBlock::GetLessFreeBlock() noexcept
    -> Pointer<MemoryDomainFreeGenericBlock>
{
    return free_less_block_;
}

void MemoryDomainFreeGenericBlock::SetLessFreeBlock(
    Pointer<MemoryDomainFreeGenericBlock> less_block) noexcept
{
    free_less_block_ = std::move(less_block);
}

auto MemoryDomainFreeGenericBlock::GetGreaterFreeBlock() const noexcept
    -> Pointer<const MemoryDomainFreeGenericBlock>
{
    return free_greater_block_;
}

auto MemoryDomainFreeGenericBlock::GetGreaterFreeBlock() noexcept
    -> Pointer<MemoryDomainFreeGenericBlock>
{
    return free_greater_block_;
}

void MemoryDomainFreeGenericBlock::SetGreaterFreeBlock(
    Pointer<MemoryDomainFreeGenericBlock> greater_block) noexcept
{
    free_greater_block_ = std::move(greater_block);
}

// MemoryDomainFreeBlockPool class

auto MemoryDomainFreeBlockPool::GetFirstFreeSmallBlock() const noexcept
    -> Pointer<MemoryDomainFreeSmallBlock>
{
    return first_small_block_;
}

auto MemoryDomainFreeBlockPool::GetFirstFreeGenericBlock() const noexcept
    -> Pointer<MemoryDomainFreeGenericBlock>
{
    return first_gen_block_;
}

auto MemoryDomainFreeBlockPool::AllocateUseBlock(MemoryDomainBlock::Size data_size,
                                                 MemoryDomainBlock::Size mem_align) noexcept
    -> Pointer<MemoryDomainUseBlock>
{
    Pointer<MemoryDomainUseBlock> used_block;

    auto block_size = MemoryDomainUseBlock::CalculateBlockSizeForData(data_size);

    if (block_size > MemoryDomainBlock::GetMinimumBlockSize() ||
        first_small_block_ == nullptr || mem_align > alignof(std::max_align_t))
        used_block = AllocateUseBlockFromSuitableGenericBlock(data_size, mem_align);
    else
        used_block = AllocateUseBlockFromSuitableSmallBlock();

    return used_block;
}

void MemoryDomainFreeBlockPool::DeallocateUseBlock(
    Pointer<MemoryDomainUseBlock>& used_block) noexcept
{
    assert(used_block != nullptr);
    if (used_block == nullptr)
        return;

    char* mem = reinterpret_cast<char*>(used_block.GetAddress());
    auto block_size = used_block->GetBlockSize();
    auto prev_block_ofs = used_block->GetPreviousBlockOffset();

    auto prev_block = used_block->GetPreviousBlock();
    if (prev_block != nullptr && IsFree(*prev_block)) {
        mem -= prev_block->GetBlockSize();
        block_size += prev_block->GetBlockSize();
        prev_block_ofs = prev_block->GetPreviousBlockOffset();

        DestroyBlock(*prev_block);
    }

    auto next_block = used_block->GetNextBlock();
    if (next_block != nullptr && IsFree(*next_block)) {
        block_size += next_block->GetBlockSize();

        DestroyBlock(*next_block);
    }

    DestroyBlock(*used_block);
    used_block.Reset();

    (void)AllocateFreeBlock(mem, block_size, prev_block_ofs);
}

void MemoryDomainFreeBlockPool::DestroyBlock(MemoryDomainBlock& block) noexcept
{
    switch (block.GetType()) {
        case MemoryDomainBlock::Type::kFreeSmall: {
            auto& free_block = static_cast<MemoryDomainFreeSmallBlock&>(block);
            DestroyBlock(free_block);

            break;
        }
        case MemoryDomainBlock::Type::kFreeGeneric: {
            auto& free_block = static_cast<MemoryDomainFreeGenericBlock&>(block);
            DestroyBlock(free_block);

            break;
        }
        case MemoryDomainBlock::Type::kUsed: {
            auto& used_block = static_cast<MemoryDomainUseBlock&>(block);
            DestroyBlock(used_block);

            break;
        }
        default:
            break;
    }
}

void MemoryDomainFreeBlockPool::DestroyBlock(MemoryDomainFreeSmallBlock& block) noexcept
{
    UnlinkFreeSmallBlock(block);
    block.~MemoryDomainFreeSmallBlock();
}

void MemoryDomainFreeBlockPool::DestroyBlock(MemoryDomainFreeGenericBlock& block) noexcept
{
    UnlinkFreeGenericBlock(block);
    block.~MemoryDomainFreeGenericBlock();
}

void MemoryDomainFreeBlockPool::DestroyBlock(MemoryDomainUseBlock& block) noexcept
{
    block.~MemoryDomainUseBlock();
}

auto MemoryDomainFreeBlockPool::AllocateUseBlockFromSuitableSmallBlock() noexcept
    -> Pointer<MemoryDomainUseBlock>
{
    if (first_small_block_ == nullptr)
        return {};

    MemoryDomainFreeSmallBlock& src_block = *first_small_block_;

    void* mem = &src_block;
    auto block_size = src_block.GetBlockSize();
    auto prev_block_ofs = src_block.GetPreviousBlockOffset();

    UnlinkFreeSmallBlock(src_block);
    src_block.~MemoryDomainFreeSmallBlock();

    Pointer<MemoryDomainUseBlock> used_block = new (mem)
        MemoryDomainUseBlock{block_size, prev_block_ofs};

    return used_block;
}

auto MemoryDomainFreeBlockPool::AllocateUseBlockFromSuitableGenericBlock(
    MemoryDomainBlock::Size data_size,
    MemoryDomainBlock::Size mem_align) noexcept -> Pointer<MemoryDomainUseBlock>
{
    auto block_size = MemoryDomainUseBlock::CalculateBlockSizeForData(data_size);

    auto [free_block, block_ofs] = FindSuitableFreeGenericBlock(block_size, mem_align);
    if (free_block == nullptr)
        return {};
    return AllocateUseBlockFromGenericBlock(*free_block, block_size, block_ofs);
}

auto MemoryDomainFreeBlockPool::FindSuitableFreeGenericBlock(
    MemoryDomainBlock::Size block_size,
    MemoryDomainBlock::Size mem_align) noexcept
    -> std::pair<MemoryDomainFreeGenericBlock*, MemoryDomainBlock::Position>
{
    if (mem_align <= MemoryDomainUseBlock::kDataAlign) {
        Pointer<MemoryDomainFreeGenericBlock> matched_block;

        for (auto free_block = first_gen_block_; free_block != nullptr;
             free_block = free_block->GetGreaterFreeBlock()) {
            auto [splittable, block_ofs] =
                free_block->CanBeSplittedForUseBlock(block_size, mem_align);
            if (splittable)
                return {free_block.GetAddress(), block_ofs};

            if ((matched_block == nullptr) && (free_block->GetBlockSize() > block_size))
                matched_block = free_block;
        }

        return {matched_block.GetAddress(), 0};
    }

    for (auto free_block = first_gen_block_; free_block != nullptr;
         free_block = free_block->GetGreaterFreeBlock()) {
        auto [splittable, block_ofs] =
            free_block->CanBeSplittedForUseBlock(block_size, mem_align);
        if (splittable)
            return {free_block.GetAddress(), block_ofs};

        /*
        for (auto sibl_free_block = free_block->GetNextFreeBlock();
             sibl_free_block != nullptr;
             sibl_free_block = sibl_free_block->GetNextFreeBlock()) {
            auto [splittable, block_ofs] =
                free_block->CanBeSplittedForUseBlock(block_size, mem_align);
            if (splittable)
                return {free_block.GetAddress(), block_ofs};
        }
        */
    }

    return {nullptr, 0};
}

auto MemoryDomainFreeBlockPool::AllocateUseBlockFromGenericBlock(
    MemoryDomainFreeGenericBlock& src_block,
    MemoryDomainBlock::Size block_size,
    MemoryDomainBlock::Position block_ofs) noexcept -> Pointer<MemoryDomainUseBlock>
{
    assert(block_size > sizeof(MemoryDomainUseBlock));
    assert((block_size % kMemoryDomainBlockAlign) == 0);

    Pointer<MemoryDomainUseBlock> used_block;

    auto src_block_size = src_block.GetBlockSize();

    if (block_ofs == 0) {
        auto new_free_block_size = src_block_size - block_size;
        if (new_free_block_size < MemoryDomainBlock::GetMinimumBlockSize()) {
            block_size = src_block_size;
            new_free_block_size = 0;
        }

        auto prev_block_ofs = src_block.GetPreviousBlockOffset();

        void* used_block_mem = &src_block;
        bool is_last = src_block.IsLastBlock() && (new_free_block_size == 0);

        UnlinkFreeGenericBlock(src_block);
        src_block.~MemoryDomainFreeGenericBlock();

        used_block = new (used_block_mem) MemoryDomainUseBlock{block_size, prev_block_ofs, is_last};

        if (!used_block->IsLastBlock() && new_free_block_size != 0) {
            auto* new_free_block_mem = static_cast<char*>(used_block_mem) + block_size;
            auto used_block_ofs = block_size;

            (void)AllocateFreeBlock(new_free_block_mem, new_free_block_size, used_block_ofs);
        }
    } else {
        assert((block_ofs % kMemoryDomainBlockAlign) == 0);
        assert(block_ofs >= MemoryDomainBlock::GetMinimumBlockSize());

        auto new_free_block_size1 = block_ofs;
        auto new_free_block_size2 = src_block_size - (block_ofs + block_size);

        if (new_free_block_size2 < MemoryDomainBlock::GetMinimumBlockSize()) {
            block_size = src_block_size - block_ofs;
            new_free_block_size2 = 0;
        }

        auto prev_block_ofs = src_block.GetPreviousBlockOffset();

        auto* new_free_block_mem1 = reinterpret_cast<char*>(&src_block);

        auto* used_block_mem = new_free_block_mem1 + block_ofs;
        bool is_last = src_block.IsLastBlock() && (new_free_block_size2 == 0);

        UnlinkFreeGenericBlock(src_block);
        src_block.~MemoryDomainFreeGenericBlock();

        (void)AllocateFreeBlock(new_free_block_mem1, new_free_block_size1, prev_block_ofs);

        used_block = new (used_block_mem) MemoryDomainUseBlock{block_size, block_ofs, is_last};

        if (!used_block->IsLastBlock() && new_free_block_size2 != 0) {
            auto* new_free_block_mem2 = static_cast<char*>(used_block_mem) + block_size;
            auto used_block_ofs = block_size;

            (void)AllocateFreeBlock(new_free_block_mem2, new_free_block_size2, used_block_ofs);
        }
    }

    assert((used_block->GetDataSize() % MemoryDomainUseBlock::kDataSizeStep) == 0);
    return used_block;
}

auto MemoryDomainFreeBlockPool::AddFreeMemoryArea(void* mem,
                                                  MemoryDomainBlock::Size mem_size) noexcept
    -> std::pair<Pointer<MemoryDomainFreeBlock>, Pointer<MemoryDomainRedZoneBlock>>
{
    bool mem_valid = (mem != nullptr &&
                      ((reinterpret_cast<uintptr_t>(mem) % kMemoryDomainBlockAlign) == 0));
    assert(mem_valid);
    if (!mem_valid)
        return {};

    auto free_block_size = CalculateInitialFreeBlockSize(mem_size);

    bool mem_size_valid = (free_block_size != 0);
    if (!mem_size_valid)
        return {};

    char* redzone_block_mem = static_cast<char*>(mem) + free_block_size;
    assert((reinterpret_cast<uintptr_t>(redzone_block_mem) % kMemoryDomainBlockAlign) == 0);

    auto prev_block_ofs = free_block_size;

    Pointer<MemoryDomainRedZoneBlock> redzone_block = new (redzone_block_mem)
        MemoryDomainRedZoneBlock{prev_block_ofs};

    auto free_block = AllocateFreeBlock(mem, free_block_size, 0);

    return {free_block, redzone_block};
}

auto MemoryDomainFreeBlockPool::ShrinkMemoryAreaFrom(
    Pointer<MemoryDomainRedZoneBlock>& redzone_block) noexcept
    -> Pointer<MemoryDomainRedZoneBlock>
{
    assert(redzone_block != nullptr);
    if (redzone_block == nullptr)
        return {};

    auto prev_block = redzone_block->GetPreviousBlock();
    if (prev_block == nullptr || IsUsed(*prev_block))
        return redzone_block;

    char* mem = reinterpret_cast<char*>(redzone_block.GetAddress());
    MemoryDomainBlock::Size prev_block_ofs{0};

    do {
        mem -= prev_block->GetBlockSize();
        prev_block_ofs = prev_block->GetPreviousBlockOffset();

        auto del_block = prev_block;
        prev_block = prev_block->GetPreviousBlock();

        DestroyBlock(*del_block);
    } while (prev_block != nullptr && IsFree(*prev_block));

    redzone_block->~MemoryDomainRedZoneBlock();
    redzone_block.Reset();

    return {new (mem) MemoryDomainRedZoneBlock{prev_block_ofs}};
}

auto MemoryDomainFreeBlockPool::CalculateInitialFreeBlockSize(
    MemoryDomainBlock::Size mem_size) noexcept -> MemoryDomainBlock::Size
{
    if (mem_size <
        (MemoryDomainBlock::GetMinimumBlockSize() + MemoryDomainRedZoneBlock::kBlockSize))
        return 0;

    auto loc_size = mem_size - MemoryDomainRedZoneBlock::kBlockSize;

    if (loc_size <= MemoryDomainUseBlock::GetMaximumDataSize()) {
        if ((loc_size / MemoryDomainBlock::GetMinimumBlockSize()) == 1)
            return MemoryDomainBlock::GetMinimumBlockSize();
    } else {
        loc_size = MemoryDomainUseBlock::GetMaximumDataSize();
    }

    auto block_size = loc_size - (loc_size % kMemoryDomainBlockAlign);
    return block_size;
}

auto MemoryDomainFreeBlockPool::AllocateFreeBlock(
    void* mem,
    MemoryDomainBlock::Size block_size,
    MemoryDomainBlock::Size prev_block_ofs) noexcept -> Pointer<MemoryDomainFreeBlock>
{
    assert((reinterpret_cast<uintptr_t>(mem) % kMemoryDomainBlockAlign) == 0);
    assert(block_size >= MemoryDomainBlock::GetMinimumBlockSize());

    if (block_size > MemoryDomainBlock::GetMinimumBlockSize())
        return AllocateFreeGenericBlock(mem, block_size, prev_block_ofs);
    else if (block_size == MemoryDomainBlock::GetMinimumBlockSize())
        return AllocateFreeSmallBlock(mem, prev_block_ofs);

    //__builtin_unreachable();
    return {};
}

auto MemoryDomainFreeBlockPool::AllocateFreeSmallBlock(
    void* mem,
    MemoryDomainBlock::Size prev_block_ofs) noexcept -> Pointer<MemoryDomainFreeSmallBlock>
{
    Pointer<MemoryDomainFreeSmallBlock> new_free_block = new (mem)
        MemoryDomainFreeSmallBlock(prev_block_ofs);

    auto next_block = new_free_block->GetNextBlock();
    if (next_block != nullptr)
        next_block->SetPreviousBlockOffset(new_free_block->GetBlockSize());

    LinkFreeSmallBlock(*new_free_block);

    return new_free_block;
}

auto MemoryDomainFreeBlockPool::AllocateFreeGenericBlock(
    void* mem,
    MemoryDomainBlock::Size block_size,
    MemoryDomainBlock::Size prev_block_ofs) noexcept -> Pointer<MemoryDomainFreeGenericBlock>
{
    assert(block_size > MemoryDomainBlock::GetMinimumBlockSize());

    Pointer<MemoryDomainFreeGenericBlock> new_free_block = new (mem)
        MemoryDomainFreeGenericBlock{block_size, prev_block_ofs};

    auto next_block = new_free_block->GetNextBlock();
    if (next_block != nullptr)
        next_block->SetPreviousBlockOffset(block_size);

    LinkFreeGenericBlock(*new_free_block);

    return new_free_block;
}

void MemoryDomainFreeBlockPool::LinkFreeSmallBlock(
    MemoryDomainFreeSmallBlock& new_block) noexcept
{
    new_block.SetPreviousFreeBlock(nullptr);
    new_block.SetNextFreeBlock(first_small_block_);

    if (first_small_block_ != nullptr)
        first_small_block_->SetPreviousFreeBlock(&new_block);

    first_small_block_ = &new_block;
}

void MemoryDomainFreeBlockPool::UnlinkFreeSmallBlock(
    MemoryDomainFreeSmallBlock& block) noexcept
{
    if (first_small_block_ == &block) {
        first_small_block_ = first_small_block_->GetNextFreeBlock();

        if (first_small_block_ != nullptr)
            first_small_block_->SetPreviousFreeBlock(nullptr);
    }

    block.UnlinkFreeBlockRelations();
}

void MemoryDomainFreeBlockPool::LinkFreeGenericBlock(
    MemoryDomainFreeGenericBlock& new_block) noexcept
{
    if (first_gen_block_ == nullptr) {
        new_block.SetLessFreeBlock(nullptr);
        new_block.SetGreaterFreeBlock(nullptr);

        first_gen_block_ = &new_block;
        return;
    }

    for (Pointer<MemoryDomainFreeGenericBlock> node_block = first_gen_block_; /**/;
         node_block = node_block->GetGreaterFreeBlock()) {
        if (node_block->GetBlockSize() == new_block.GetBlockSize()) {
            AddEqualFreeGenericBlock(*node_block, new_block);
            return;
        } else if (node_block->GetBlockSize() > new_block.GetBlockSize()) {
            InsertBeforeFreeGenericBlock(*node_block, new_block);
            return;
        } else if (node_block->GetGreaterFreeBlock() == nullptr) {
            InsertAfterFreeGenericBlock(*node_block, new_block);
            return;
        }
    }
}

void MemoryDomainFreeBlockPool::AddEqualFreeGenericBlock(
    MemoryDomainFreeGenericBlock& src_block,
    MemoryDomainFreeGenericBlock& new_block) noexcept
{
    auto less_block = src_block.GetLessFreeBlock();
    auto greater_block = src_block.GetGreaterFreeBlock();

    new_block.SetLessFreeBlock(less_block);
    new_block.SetGreaterFreeBlock(greater_block);

    if (less_block != nullptr)
        less_block->SetGreaterFreeBlock(&new_block);
    src_block.SetLessFreeBlock(nullptr);

    if (greater_block != nullptr)
        greater_block->SetLessFreeBlock(&new_block);
    src_block.SetGreaterFreeBlock(nullptr);

    new_block.SetPreviousFreeBlock(nullptr);
    new_block.SetNextFreeBlock(&src_block);

    src_block.SetPreviousFreeBlock(&new_block);

    if (first_gen_block_ == &src_block)
        first_gen_block_ = &new_block;
}

void MemoryDomainFreeBlockPool::InsertAfterFreeGenericBlock(
    MemoryDomainFreeGenericBlock& src_block,
    MemoryDomainFreeGenericBlock& new_block) noexcept
{
    auto greater_block = src_block.GetGreaterFreeBlock();

    new_block.SetGreaterFreeBlock(greater_block);
    if (greater_block != nullptr)
        greater_block->SetLessFreeBlock(&new_block);

    src_block.SetGreaterFreeBlock(&new_block);
    new_block.SetLessFreeBlock(&src_block);
}

void MemoryDomainFreeBlockPool::InsertBeforeFreeGenericBlock(
    MemoryDomainFreeGenericBlock& src_block,
    MemoryDomainFreeGenericBlock& new_block) noexcept
{
    auto less_block = src_block.GetLessFreeBlock();

    new_block.SetLessFreeBlock(less_block);
    new_block.SetGreaterFreeBlock(&src_block);

    src_block.SetLessFreeBlock(&new_block);

    if (less_block != nullptr)
        less_block->SetGreaterFreeBlock(&new_block);

    if (first_gen_block_ == &src_block)
        first_gen_block_ = &new_block;
}

void MemoryDomainFreeBlockPool::UnlinkFreeGenericBlock(
    MemoryDomainFreeGenericBlock& block) noexcept
{
    auto less_free_block = block.GetLessFreeBlock();
    if (less_free_block != nullptr)
        less_free_block->SetGreaterFreeBlock(block.GetGreaterFreeBlock());

    auto greater_free_block = block.GetGreaterFreeBlock();
    if (greater_free_block != nullptr)
        greater_free_block->SetLessFreeBlock(block.GetLessFreeBlock());

    if (first_gen_block_ == &block) {
        first_gen_block_ = block.GetGreaterFreeBlock();
        if (first_gen_block_ != nullptr)
            first_gen_block_->SetLessFreeBlock(nullptr);
    }

    block.UnlinkFreeBlockRelations();
}

}  // namespace sme
