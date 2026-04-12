#ifndef SME_MEM_SPACE_H
#define SME_MEM_SPACE_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

#include "sme/basic_allocator.h"
#include "sme/internal/mem_space_manip.h"
#include "sme/mem_ptr.h"
#include "sme/sme_export.h"
#include "sme/sync.h"

// NOLINTBEGIN(modernize-use-nodiscard, google-explicit-constructor)

namespace sme {

class SME_EXPORT MemorySpace final {
   public:
    class Iterator;
    struct AllocationInfo;

   public:
    MemorySpace(const Pointer<void>& mem,
                size_t size,
                Synchronizer::Type sync_type = Synchronizer::Type::kNone);

    MemorySpace(const MemorySpace&) = delete;
    MemorySpace(MemorySpace&& obj) noexcept = delete;
    auto operator=(const MemorySpace&) -> MemorySpace& = delete;
    auto operator=(MemorySpace&& obj) noexcept -> MemorySpace& = delete;

    ~MemorySpace() = default;

    static constexpr auto GetMinimumSpaceSize() noexcept -> size_t;

    [[nodiscard]] auto IsValid() const noexcept -> bool;

    [[nodiscard]] auto GetBaseAddress() const noexcept -> Pointer<void>;
    [[nodiscard]] auto GetCapacity() const noexcept -> size_t;

    [[nodiscard]] auto Allocate(size_t size) -> Pointer<void>;
    [[nodiscard]] auto AllocateAtLeast(size_t size) -> std::pair<Pointer<void>, size_t>;
    [[nodiscard]] auto Resize(Pointer<void> ptr, size_t new_size) -> bool;

    auto Deallocate(Pointer<void>& ptr) noexcept -> bool;
    auto Deallocate(Pointer<void>&& ptr) noexcept -> bool;

    [[nodiscard]] auto GetAllocationInfo(const Pointer<void>& ptr) const
        -> std::optional<AllocationInfo>;

    [[nodiscard]] auto begin() const noexcept -> Iterator;
    [[nodiscard]] auto end() const noexcept -> Iterator;

   public:
    struct AllocationInfo {
        void* block{nullptr};
        void* data{nullptr};
        size_t position{0};
        size_t size{0};
        bool free{true};
    };

    class Iterator {
       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = AllocationInfo;
        using reference = AllocationInfo&;
        using pointer = AllocationInfo*;
        using difference_type = std::ptrdiff_t;

        friend class MemorySpace;

       private:
        explicit Iterator(Pointer<MemorySpaceManipulator> mem_manip);

       public:
        Iterator() = default;

        [[nodiscard]] auto GetValue() const -> value_type;
        [[nodiscard]] auto operator*() const -> value_type;

        [[nodiscard]] operator bool() const noexcept;
        [[nodiscard]] auto operator !() const noexcept -> bool;

        auto operator++() -> Iterator&;
        auto operator++(int) -> Iterator;

        [[nodiscard]] auto operator==(const Iterator& other) const noexcept -> bool;
        [[nodiscard]] auto operator!=(const Iterator& other) const noexcept -> bool;

       private:
        Pointer<MemorySpaceManipulator> mem_manip_{};
        mutable Pointer<MemorySpaceBlock> iter_block_{};
    };

   private:
    using Block = MemorySpaceBlock;
    using BlockSize = Block::Size;

   private:
    auto ExpandBlock(Block& block, BlockSize block_size) noexcept -> bool;
    auto ShrinkBlock(Block& block, BlockSize block_size) noexcept -> bool;

    auto GetBlock(const Pointer<void>& ptr) const -> const Block&;
    auto GetBlock(Pointer<void>& ptr) -> Block&;

    auto GetAllocatedBlock(const Pointer<void>& data_ptr) const -> const Block&;
    auto GetAllocatedBlock(Pointer<void>& data_ptr) -> Block&;

    auto Intersects(const MemorySpaceBlock& block1,
                    const MemorySpaceBlock& block2) noexcept -> bool;

   private:
    mutable Synchronizer sync_;
    mutable MemorySpaceManipulator mem_manip_;
    Pointer<Block> curr_block_;
};

constexpr auto MemorySpace::GetMinimumSpaceSize() noexcept -> size_t
{
    return MemorySpaceBlock::GetMinBlockSize();
}

}  // namespace sme

// NOLINTEND(modernize-use-nodiscard, google-explicit-constructor)

#endif  // SME_MEM_SPACE_H
