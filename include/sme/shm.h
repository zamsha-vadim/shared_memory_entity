#ifndef SME_SHM_H
#define SME_SHM_H

#include <fcntl.h>
#include <sys/stat.h>

#include <filesystem>
#include <memory>

#include "sme/mem_map.h"
#include "sme/sme_export.h"

namespace sme {

struct SME_EXPORT MemoryMapRequest final {
    static constexpr size_t kAllSize{std::numeric_limits<size_t>::max()};

    size_t size{kAllSize};
    off_t offset{0};
    InterprocessVisibility share_type{InterprocessVisibility::kPrivate};
};

constexpr MemoryMapRequest kAllMemoryMapRequestAsShared = {
    .size = MemoryMapRequest::kAllSize,
    .offset = 0,
    .share_type = InterprocessVisibility::kShared};

constexpr MemoryMapRequest kAllMemoryMapRequestAsPrivate = {
    .size = MemoryMapRequest::kAllSize,
    .offset = 0,
    .share_type = InterprocessVisibility::kPrivate};

class SME_EXPORT SharedMemoryFile final {
   public:
    using Flags = int;
    using Mode = ::mode_t;

   public:
    SharedMemoryFile(std::filesystem::path path,
                     Flags flags,
                     Mode mode = S_IRUSR | S_IWUSR,
                     size_t size = {});

    SharedMemoryFile();

    SharedMemoryFile(const SharedMemoryFile& other);
    SharedMemoryFile(SharedMemoryFile&& other) noexcept;
    auto operator=(const SharedMemoryFile& other) -> SharedMemoryFile&;
    auto operator=(SharedMemoryFile&& other) noexcept -> SharedMemoryFile&;

    ~SharedMemoryFile();

    [[nodiscard]] auto IsValid() const noexcept -> bool;

    auto GetPath() const -> const std::filesystem::path&;

    auto GetSize() const -> size_t;
    void SetSize(size_t size);

    void Close() noexcept;

    static void Unlink(const std::filesystem::path& path);
    static void UnlinkIfExists(const std::filesystem::path& path);

    [[nodiscard]] auto MapMemory(
        const MemoryMapRequest& req = kAllMemoryMapRequestAsPrivate) const -> MemoryMap;
    [[nodiscard]] static auto MapAnonymousMemory(
        size_t size,
        InterprocessVisibility share_type = InterprocessVisibility::kPrivate)
        -> MemoryMap;

   private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace sme

#endif  // SME_SHM_H
