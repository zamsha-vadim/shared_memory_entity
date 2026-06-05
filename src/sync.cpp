#include "sme/sync.h"

#include <cassert>
#include <system_error>

namespace sme {

namespace {

using ImplementerType = std::variant<std::monostate, Mutex, AdaptiveSpinLock>;

void InitializeImplementer(ImplementerType& impl, SynchronizationType sync_type)
{
    switch (sync_type) {
        case SynchronizationType::kPrivateMutex:
            impl.emplace<Mutex>(InterprocessVisibility::kPrivate);
            break;
        case SynchronizationType::kSharedMutex:
            impl.emplace<Mutex>(InterprocessVisibility::kShared);
            break;
        case SynchronizationType::kAdaptiveSpinlock:
            impl.emplace<AdaptiveSpinLock>();
            break;
        default:
        case SynchronizationType::kNone:
            break;
    }
}

}  // namespace

Synchronizer::Synchronizer(SynchronizationType type) : type_{type}
{
    InitializeImplementer(impl_, type_);
}

void Synchronizer::lock()
{
    switch (type_) {
        case SynchronizationType::kPrivateMutex:
        case SynchronizationType::kSharedMutex:
            std::get<1>(impl_).lock();
            break;
        case SynchronizationType::kAdaptiveSpinlock:
            std::get<2>(impl_).lock();
            break;
        default:
        case SynchronizationType::kNone:
            break;
    }
}

void Synchronizer::unlock()
{
    switch (type_) {
        case SynchronizationType::kPrivateMutex:
        case SynchronizationType::kSharedMutex:
            std::get<1>(impl_).unlock();
            break;
        case SynchronizationType::kAdaptiveSpinlock:
            std::get<2>(impl_).unlock();
            break;
        default:
        case SynchronizationType::kNone:
            break;
    }
}

}  // namespace sme
