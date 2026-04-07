#ifndef SME_BASIC_ALLOCATOR_TCC
#define SME_BASIC_ALLOCATOR_TCC

#include <cassert>
#include <memory>
#include <stdexcept>

namespace sme {

// class AllocationContext implementation

template <typename MemoryAreaType>
thread_local Pointer<MemoryAreaType> AllocationContext<MemoryAreaType>::glob_area_;

template <typename MemoryAreaType>
AllocationContext<MemoryAreaType>::AllocationContext(MemoryAreaType& area)
{
    Acquire(area);
}

template <typename MemoryAreaType>
AllocationContext<MemoryAreaType>::~AllocationContext()
{
    if (acquired_)
        glob_area_.Reset();
}

template <typename MemoryAreaType>
void AllocationContext<MemoryAreaType>::Acquire(MemoryAreaType& area)
{
    if (acquired_) {
        if (area_ == &area)
            return;
    } else if (!CanAcquire()) {
        throw std::logic_error("Already acquired another allocator context");
    }

    area_ = &area;
    glob_area_ = &area;

    acquired_ = true;
}

template <typename MemoryAreaType>
void AllocationContext<MemoryAreaType>::Acquire()
{
    Acquire(*area_);
}

template <typename MemoryAreaType>
void AllocationContext<MemoryAreaType>::Release()
{
    if (!acquired_)
        throw std::logic_error("Allocator context not acquired");

    glob_area_.Reset();

    acquired_ = false;
}

template <typename MemoryAreaType>
auto AllocationContext<MemoryAreaType>::IsAcquired() noexcept -> bool
{
    return acquired_;
}

template <typename MemoryAreaType>
auto AllocationContext<MemoryAreaType>::CanAcquire() noexcept -> bool
{
    return (glob_area_ == nullptr);
}

template <typename MemoryAreaType>
auto AllocationContext<MemoryAreaType>::GetArea() const noexcept -> AreaPointer
{
    return (acquired_ ? area_ : AreaPointer{});
}

template <typename MemoryAreaType>
auto AllocationContext<MemoryAreaType>::GetCurrentArea() noexcept -> AreaPointer
{
    return glob_area_;
}

template <typename MemoryAreaType>
auto AllocationContext<MemoryAreaType>::IsPresented() noexcept -> bool
{
    return (glob_area_ != nullptr);
}

// class BasicAllocator implementation

template <typename T, typename MemoryAreaType>
BasicAllocator<T, MemoryAreaType>::BasicAllocator()
{
    auto curr_area = AllocationContext<MemoryAreaType>::GetCurrentArea();
    if (curr_area == nullptr)
        throw std::runtime_error("None default allocation area");

    area_ = std::move(curr_area);
}

template <typename T, typename MemoryAreaType>
BasicAllocator<T, MemoryAreaType>::BasicAllocator(MemoryAreaType& area) : area_{&area}
{
}

template <typename T, typename MemoryAreaType>
template <typename U>
BasicAllocator<T, MemoryAreaType>::BasicAllocator(
    const BasicAllocator<U, MemoryAreaType>& other) noexcept
    : area_{other.GetMemoryArea()}
{
}

template <typename T, typename MemoryAreaType>
template <typename U>
BasicAllocator<T, MemoryAreaType>::BasicAllocator(
    BasicAllocator<U, MemoryAreaType>&& other) noexcept
    : area_{other.GetMemoryArea()}
{
}

template <typename T, typename MemoryAreaType>
template <typename U>
auto BasicAllocator<T, MemoryAreaType>::operator=(
    const BasicAllocator<U, MemoryAreaType>& other) noexcept -> BasicAllocator&
{
    area_ = other.GetMemoryArea();
    return *this;
}

template <typename T, typename MemoryAreaType>
template <typename U>
auto BasicAllocator<T, MemoryAreaType>::operator=(
    BasicAllocator<U, MemoryAreaType>&& other) noexcept -> BasicAllocator&
{
    area_ = other.GetMemoryArea();
    return *this;
}

template <typename T, typename MemoryAreaType>
auto BasicAllocator<T, MemoryAreaType>::allocate(size_type size) -> pointer
{
    assert(IsValid());
    if (!IsValid())
        throw std::logic_error("Invalid memory allocator");

    auto raw_size{sizeof(value_type) * size};

    auto ptr = area_->Allocate(raw_size);
    if (ptr == nullptr)
        throw std::bad_alloc();

    assert(alignof(value_type) == 0 ||
           (reinterpret_cast<uintptr_t>(ptr.GetAddress()) % alignof(value_type)) == 0);

    return ptr;
}

/*
template <typename T, typename MemoryAreaType>
auto BasicAllocator<T, MemoryAreaType>::allocate(size_type size, const void* hint)
    -> pointer {
    assert(size == 0);
    return nullptr;
}
*/

template <typename T, typename MemoryAreaType>
void BasicAllocator<T, MemoryAreaType>::deallocate(pointer ptr,
                                                   [[maybe_unused]] size_type) noexcept
{
    if (!ptr)
        return;

    assert(IsValid());
    if (!IsValid())
        return;

    Pointer<void> ms_ptr = ptr;
    area_->Deallocate(ms_ptr);
}

template <typename T, typename MemoryAreaType>
void BasicAllocator<T, MemoryAreaType>::construct(pointer ptr, const_reference val)
{
    assert(ptr != nullptr);
    assert(alignof(value_type) == 1 ||
           (reinterpret_cast<uintptr_t>(ptr.GetAddress()) % alignof(value_type)) == 0);

    if (ptr == nullptr)
        throw std::invalid_argument("Memory pointer is null");
    if (alignof(value_type) != 1 &&
        (reinterpret_cast<uintptr_t>(ptr.GetAddress()) % alignof(value_type)) != 0) {
        throw std::invalid_argument("Memory pointer is not aligned for type");
    }

    new (ptr.GetAddress()) T{val};
}

}  // namespace sme

#endif  // SME_BASIC_ALLOCATOR_TCC
