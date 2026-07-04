#ifndef SME_INTERNAL_LIKELY_OPER_H
#define SME_INTERNAL_LIKELY_OPER_H

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#endif  // SME_INTERNAL_LIKELY_OPER_H
