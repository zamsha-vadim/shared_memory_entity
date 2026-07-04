#ifndef SME_INTERNAL_UTIL_H
#define SME_INTERNAL_UTIL_H

#if defined(__x86_64__)
#include <x86intrin.h>
#endif

#include "sme/internal/likely_oper.h"
#include "sme/internal/type.h"

namespace sme {

inline void RelaxCpu()
{
#if defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#endif
}

}  // namespace sme

#endif
