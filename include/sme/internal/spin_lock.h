#ifndef SME_SPIN_LOCK_H
#define SME_SPIN_LOCK_H

namespace sme {

struct SpinLock {
    void Lock() {}
    void Unlock() {}
};

}  // namespace sme

#endif // SME_SPIN_LOCK_H

