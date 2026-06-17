#ifndef SME_INTERNAL_TYPE_H
#define SME_INTERNAL_TYPE_H

#include <cstddef>
#include <new>

#if defined(__x86_64__)
#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#error \
    " the target processor doen't support atomic compare and swap operations on operands 16 bytes in length"
#endif
#endif

namespace sme {

constexpr auto kDWCASAlign{16U};

#ifdef __cpp_lib_hardware_interference_size
constexpr size_t kCacheLineSize = std::hardware_constructive_interference_size;
#else
constexpr size_t kCacheLineSize{64};
#endif

}  // namespace sme

#endif //SME_INTERNAL_TYPE_H

