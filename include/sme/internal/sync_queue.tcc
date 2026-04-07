#ifndef SME_SYNC_QUEUE_TCC
#define SME_SYNC_QUEUE_TCC

namespace sme {

template <typename T>
SynchronizedQueue<T>::SynchronizedQueue(SyncronizationType sync_type)
{
    switch (sync_type) {
        case SyncronizationType::kSpin:
            sync_ = SpinLock{};
            break;
        default:
            break;
    }
}

template <typename T>
SynchronizedQueue<T>::~SynchronizedQueue()
{
}

template <typename T>
void SynchronizedQueue<T>::Write(ItemPointer obj)
{
    std::visit(
        [this, &obj](auto& sync) {
            sync.Lock();

            impl_.Write(obj);
            
            sync.Unlock();
        },
        sync_);
}

template <typename T>
auto SynchronizedQueue<T>::Read() -> ItemPointer
{
    return std::visit(
        [this](auto& sync) -> auto {
            sync.Lock();

            auto obj = impl_.Read();
            
            sync.Unlock();
            return obj;
        },
        sync_);
}

template <typename T>
auto SynchronizedQueue<T>::WaitForReadAvailability() -> bool
{
    return false;
}

template <typename T>
auto SynchronizedQueue<T>::IsEmpty() const -> bool
{
    return std::visit(
        [this](auto& sync) -> auto {
            sync.Lock();

            bool is_empty = impl_.IsEmpty();
            
            sync.Unlock();
            return is_empty;
        },
        sync_);
}

}  // namespace sme

#endif // SME_SYNC_QUEUE_TCC
