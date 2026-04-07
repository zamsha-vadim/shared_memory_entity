#include "sme/shm.h"

#include <sys/mman.h>

#include <cassert>
#include <cerrno>
#include <system_error>

// NOLINTBEGIN(bugprone-empty-catch)

namespace sme {

namespace {

auto MapIntoMemory(int file_desc,
                   size_t size,
                   off_t offset,
                   MemoryMap::ShareType share_type) -> void*
{
    int flags{0};

    if (file_desc == -1)
        flags |= MAP_ANONYMOUS;
    flags |= (share_type == MemoryMap::ShareType::kShared ? MAP_SHARED : MAP_PRIVATE);

    auto* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, file_desc, offset);
    if (addr == MAP_FAILED)
        throw std::system_error(errno, std::generic_category(), "Memory map error");

    return addr;
}

}  // namespace

MemoryMap::MemoryMap(int file_desc, size_t size, off_t offset, ShareType share_type)
    : addr_{MapIntoMemory(file_desc, size, offset, share_type)}, size_{size},
      share_type_{share_type}
{
}

MemoryMap::MemoryMap(MemoryMap&& other) noexcept
{
    Move(other);
}

auto MemoryMap::operator=(MemoryMap&& other) noexcept -> MemoryMap&
{
    Move(other);
    return *this;
}

MemoryMap::~MemoryMap()
{
    try {
        Unmap();
    } catch (...) {
    }
}

void MemoryMap::Move(MemoryMap& other) noexcept
{
    if (this == &other)
        return;

    if (addr_ != nullptr) {
        [[maybe_unused]] auto res = munmap(addr_, size_);
        assert(res == 0);
    }

    addr_ = other.addr_;
    size_ = other.size_;
    share_type_ = other.share_type_;

    other.addr_ = nullptr;
    other.size_ = 0;
    other.share_type_ = ShareType::kPrivate;
}

auto MemoryMap::isValid() const noexcept -> bool
{
    return (addr_ != nullptr && size_ != 0);
}

auto MemoryMap::GetAddress() const noexcept -> const void*
{
    return addr_;
}

MemoryMap::operator const void*() const noexcept
{
    return GetAddress();
}

auto MemoryMap::GetAddress() noexcept -> void*
{
    return addr_;
}

MemoryMap::operator void*() noexcept
{
    return GetAddress();
}

auto MemoryMap::GetSize() const noexcept -> size_t
{
    return size_;
}

auto MemoryMap::IsShared() const noexcept -> bool {
    return share_type_ == ShareType::kShared;
}

void MemoryMap::Unmap()
{
    if (addr_ == nullptr)
        return;

    auto res = munmap(addr_, size_);
    if (res != 0)
        throw std::system_error(errno, std::generic_category(), "Memory unmap error");

    addr_ = nullptr;
    size_ = 0;
    share_type_ = ShareType::kPrivate;
}

}  // namespace sme

// NOLINTEND(bugprone-empty-catch)
