#include "sme/sync.h"

#include <cassert>
#include <system_error>

namespace sme {

namespace {

auto CreateOptionalMutex(Synchronizer::Type sync_type) -> std::optional<Mutex>
{
    switch (sync_type) {
        case Synchronizer::Type::kNone:
            return {};

        case Synchronizer::Type::kPrivate:
            return std::optional<Mutex>{std::in_place, InterprocessVisibility::kPrivate};

        case Synchronizer::Type::kShared:
            return std::optional<Mutex>{std::in_place, InterprocessVisibility::kShared};

        default:
            throw std::logic_error("Not supported synchronization type");
    }
}

}  // namespace

Synchronizer::Synchronizer(Type type) : mutex_{CreateOptionalMutex(type)} {}

void Synchronizer::lock()
{
    if (mutex_)
        mutex_->lock();
}

auto Synchronizer::try_lock() -> bool
{
    return (mutex_ ? mutex_->try_lock() : true);
}

void Synchronizer::unlock()
{
    if (mutex_)
        mutex_->unlock();
}

}  // namespace sme

