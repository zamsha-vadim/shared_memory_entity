#ifndef SME_INTERNAL_TYPE_H
#define SME_INTERNAL_TYPE_H

#include <cstddef>
#include <new>

namespace sme {

constexpr auto kDWCASAlign{16U};

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t kCacheLineSize = std::hardware_constructive_interference_size;
#else
constexpr size_t kCacheLineSize{64};
#endif

namespace internal {

template <typename T>
constexpr T* launder(T* p) noexcept
{
#if defined(__clang__)
    return p;
#else
    return std::launder(p);
#endif
}

}  // namespace internal
}  // namespace sme

#endif  // SME_INTERNAL_TYPE_H

