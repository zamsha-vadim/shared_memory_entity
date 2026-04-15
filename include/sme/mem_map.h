#ifndef SME_MEM_MAP_H
#define SME_MEM_MAP_H

#include <sys/types.h>

#include <cstddef>
#include <limits>

#include "sme/ipvt.h"
#include "sme/sme_export.h"

namespace sme {

class SME_EXPORT MemoryMap final {
    friend class SharedMemoryFile;

   public:
    MemoryMap() = default;

    MemoryMap(const MemoryMap& other) = delete;
    MemoryMap(MemoryMap&& other) noexcept;
    auto operator=(const MemoryMap& other) -> MemoryMap& = delete;
    auto operator=(MemoryMap&& other) noexcept -> MemoryMap&;

    ~MemoryMap();

    [[nodiscard]] auto isValid() const noexcept -> bool;

    auto GetAddress() const noexcept -> const void*;
    operator const void*() const noexcept;  // NOLINT(google-explicit-constructor)

    auto GetAddress() noexcept -> void*;
    operator void*() noexcept;  // NOLINT(google-explicit-constructor)

    auto GetSize() const noexcept -> size_t;

    auto IsShared() const noexcept -> bool;

    void Unmap();

   private:
    MemoryMap(int file_desc,
              size_t size,
              off_t offset,
              InterprocessVisibility share_type);

    void Move(MemoryMap& other) noexcept;

   private:
    void* addr_{nullptr};
    size_t size_{0};
    InterprocessVisibility share_type_{InterprocessVisibility::kPrivate};
};

}  // namespace sme

#endif  // SME_MEM_MAP_H
