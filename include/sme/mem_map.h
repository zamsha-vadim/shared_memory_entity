#ifndef SME_MEM_MAP_H
#define SME_MEM_MAP_H

#include <sys/types.h>

#include <cstddef>
#include <limits>

#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT MemoryMap final {
    friend class SharedMemoryFile;

   public:
    enum class ShareType { kPrivate, kShared };

   public:
    MemoryMap() = default;

    MemoryMap(const MemoryMap& other) = delete;
    MemoryMap(MemoryMap&&) noexcept;
    auto operator=(const MemoryMap&) -> MemoryMap& = delete;
    auto operator=(MemoryMap&& other) noexcept -> MemoryMap&;

    ~MemoryMap();

    [[nodiscard]] auto isValid() const noexcept -> bool;

    auto GetAddress() const noexcept -> const void*;
    operator const void*() const noexcept;

    auto GetAddress() noexcept -> void*;
    operator void*() noexcept;

    auto GetSize() const noexcept -> size_t;

    auto IsShared() const noexcept -> bool;

    void Unmap();

   private:
    MemoryMap(int file_desc, size_t size, off_t offset, ShareType share_type);

    void Move(MemoryMap&) noexcept;

   private:
    void* addr_{nullptr};
    size_t size_{0};
    ShareType share_type_{ShareType::kPrivate};
};

}  // namespace sme

#endif  // SME_MEM_MAP_H
