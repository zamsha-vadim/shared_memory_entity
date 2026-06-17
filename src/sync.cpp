#include "sme/sync.h"

#include <cassert>
#include <system_error>

namespace sme {

namespace {

using ImplementerType = std::variant<AdaptiveSpinLock, Mutex, std::monostate>;

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

auto Synchronizer::GetType() const noexcept -> SynchronizationType
{
    return type_;
}

void Synchronizer::lock()
{
    switch (type_) {
        case SynchronizationType::kAdaptiveSpinlock:
            std::get<0>(impl_).lock();
            return;

        case SynchronizationType::kPrivateMutex:
        case SynchronizationType::kSharedMutex:
            std::get<1>(impl_).lock();
            return;
        
        case SynchronizationType::kNone:
        default:
            return;
    }
}

void Synchronizer::unlock()
{
    switch (type_) {
        case SynchronizationType::kAdaptiveSpinlock:
            std::get<0>(impl_).unlock();
            return;

        case SynchronizationType::kPrivateMutex:
        case SynchronizationType::kSharedMutex:
            std::get<1>(impl_).unlock();
            return;
        
        case SynchronizationType::kNone:
        default:
            return;
    }
}

}  // namespace sme
