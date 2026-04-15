#include "sme/shm.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <system_error>

// NOLINTBEGIN(bugprone-easily-swappable-parameters)

namespace sme {

namespace {

constexpr int kInvalidFileDescriptor{-1};

[[noreturn]] void ThrowNotOpen()
{
    throw std::runtime_error("Shared memory not open");
}

[[noreturn]] void ThrowSystemError(int err, const std::string& descr = {})
{
    static const std::string kErrPrefix{"Shared memory error"};

    throw std::system_error(err, std::generic_category(),
                            !descr.empty() ? descr : kErrPrefix);
}

}  // namespace

// SharedMemoryFile::Impl class

class SharedMemoryFile::Impl {
   public:
    Impl(std::filesystem::path path, Flags flags, Mode mode);

    Impl(const Impl& other);
    Impl(Impl&& other) noexcept;
    auto operator=(const Impl& other) -> Impl&;
    auto operator=(Impl&& other) noexcept -> Impl&;

    ~Impl();

    auto IsValid() const noexcept -> bool;

    auto GetPath() const noexcept -> const std::filesystem::path&;

    auto GetSize() const -> size_t;
    void SetSize(size_t size);

    auto MapMemory(const MemoryMapRequest& req) const -> MemoryMap;
    static auto MapAnonymousMemory(size_t size, InterprocessVisibility share_type)
        -> MemoryMap;

   private:
    void Copy(const Impl& other);
    void Move(Impl& other) noexcept;

    void Close() noexcept;

   private:
    std::filesystem::path path_;
    int fd_{kInvalidFileDescriptor};
};

SharedMemoryFile::Impl::Impl(std::filesystem::path path, Flags flags, Mode mode)
    : path_{std::move(path)}
{
    // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
    fd_ = shm_open(path_.c_str(), flags, mode);
    if (fd_ < 0)
        ThrowSystemError(errno, "Shared memory open error");
}

SharedMemoryFile::Impl::Impl(const Impl& other)
{
    Copy(other);
}

SharedMemoryFile::Impl::Impl(Impl&& other) noexcept
{
    Move(other);
}

auto SharedMemoryFile::Impl::operator=(const Impl& other) -> Impl&
{
    Copy(other);
    return *this;
}

auto SharedMemoryFile::Impl::operator=(Impl&& other) noexcept -> Impl&
{
    Move(other);
    return *this;
}

SharedMemoryFile::Impl::~Impl()
{
    Close();
}

auto SharedMemoryFile::Impl::IsValid() const noexcept -> bool
{
    return fd_ != kInvalidFileDescriptor;
}

auto SharedMemoryFile::Impl::GetPath() const noexcept -> const std::filesystem::path&
{
    return path_;
}

auto SharedMemoryFile::Impl::GetSize() const -> size_t
{
    struct stat info {};

    auto res = fstat(fd_, &info);
    if (res == -1)
        ThrowSystemError(errno, "Can't get size of shared memory");

    return info.st_size;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void SharedMemoryFile::Impl::SetSize(size_t size)
{
    auto res = ftruncate(fd_, static_cast<off_t>(size));
    if (res == -1)
        ThrowSystemError(errno, "Can't set size of shared memory");
}

void SharedMemoryFile::Impl::Close() noexcept
{
    if (!IsValid())
        return;

    close(fd_);

    fd_ = kInvalidFileDescriptor;
    path_.clear();
}

auto SharedMemoryFile::Impl::MapMemory(const MemoryMapRequest& req) const -> MemoryMap
{
    if (!IsValid())
        throw std::logic_error("Shared memory file closed");

    auto mapped_size = (req.size != MemoryMapRequest::kAllSize ? req.size : GetSize());

    return MemoryMap{fd_, mapped_size, req.offset, req.share_type};
}

auto SharedMemoryFile::Impl::MapAnonymousMemory(size_t size,
                                                InterprocessVisibility share_type)
    -> MemoryMap
{
    return MemoryMap{kInvalidFileDescriptor, size, 0, share_type};
}

void SharedMemoryFile::Impl::Copy(const Impl& other)
{
    if (this == &other)
        return;

    if (other.IsValid()) {
        auto dup_fd = dup(other.fd_);
        if (dup_fd == kInvalidFileDescriptor)
            ThrowSystemError(errno, "Can't copy shared memory object");

        Close();

        fd_ = dup_fd;
        path_ = other.path_;
    } else {
        Close();
    }
}

void SharedMemoryFile::Impl::Move(Impl& other) noexcept
{
    if (this == &other)
        return;

    Close();

    fd_ = other.fd_;
    path_ = std::move(other.path_);

    other.fd_ = kInvalidFileDescriptor;
}

// SharedMemoryFile class

SharedMemoryFile::SharedMemoryFile() = default;

SharedMemoryFile::SharedMemoryFile(std::filesystem::path path,
                                   Flags flags,
                                   Mode mode,
                                   size_t size)
    : pimpl_{std::make_unique<Impl>(std::move(path), flags, mode)}
{
    if (size != 0 && ((flags & O_CREAT) == O_CREAT))
        SetSize(size);
}

SharedMemoryFile::SharedMemoryFile(const SharedMemoryFile& other)
{
    if (other.IsValid())
        pimpl_ = std::make_unique<Impl>(*(other.pimpl_));
}

SharedMemoryFile::SharedMemoryFile(SharedMemoryFile&& other) noexcept
    : pimpl_{std::move(other.pimpl_)}
{
}

auto SharedMemoryFile::operator=(const SharedMemoryFile& other) -> SharedMemoryFile&
{
    if (&other != this) {
        if (other.IsValid()) {
            if (pimpl_->IsValid())
                *pimpl_ = *(other.pimpl_);
            else
                pimpl_ = std::make_unique<Impl>(*(other.pimpl_));
        } else {
            pimpl_.reset();
        }
    }
    return *this;
}

auto SharedMemoryFile::operator=(SharedMemoryFile&& other) noexcept -> SharedMemoryFile&
{
    if (&other != this)
        pimpl_ = std::move(other.pimpl_);
    return *this;
}

SharedMemoryFile::~SharedMemoryFile() = default;

auto SharedMemoryFile::IsValid() const noexcept -> bool
{
    return pimpl_ != nullptr && pimpl_->IsValid();
}

auto SharedMemoryFile::GetPath() const -> const std::filesystem::path&
{
    if (!IsValid())
        ThrowNotOpen();
    return pimpl_->GetPath();
}

auto SharedMemoryFile::GetSize() const -> size_t
{
    if (!IsValid())
        ThrowNotOpen();
    return pimpl_->GetSize();
}

void SharedMemoryFile::SetSize(size_t size)
{
    if (!IsValid())
        ThrowNotOpen();
    pimpl_->SetSize(size);
}

void SharedMemoryFile::Close() noexcept
{
    pimpl_.reset();
}

void SharedMemoryFile::Unlink(const std::filesystem::path& path)
{
    auto res = shm_unlink(path.c_str());
    if (res == -1)
        ThrowSystemError(errno, std::string("Can't unlink the shared memory file '") +
                                    path.c_str() + "'");
}

void SharedMemoryFile::UnlinkIfExists(const std::filesystem::path& path)
{
    auto res = shm_unlink(path.c_str());
    if (res == 0 || errno == ENOENT)
        return;
    ThrowSystemError(
        errno, std::string("Can't unlink the shared memory file '") + path.c_str() + "'");
}

auto SharedMemoryFile::MapMemory(const MemoryMapRequest& req) const -> MemoryMap
{
    if (!IsValid())
        ThrowNotOpen();
    return pimpl_->MapMemory(req);
}

auto SharedMemoryFile::MapAnonymousMemory(size_t size, InterprocessVisibility share_type)
    -> MemoryMap
{
    return Impl::MapAnonymousMemory(size, share_type);
}

}  // namespace sme

// NOLINTEND(bugprone-easily-swappable-parameters)
