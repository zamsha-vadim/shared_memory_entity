#ifndef SME_MSP_ROOT_OBJ_H
#define SME_MSP_ROOT_OBJ_H

#include <memory>
#include <new>
#include <stdexcept>

#include "sme/alloc_util.h"
#include "sme/mem_ptr.h"
#include "sme/mem_space.h"
#include "sme/sme_export.h"

namespace sme {
namespace msp {

template <typename T, typename... Arg>
[[nodiscard]] auto SME_EXPORT CreateRoot(MemorySpace& mem_space, Arg&&... args) -> T*
{
    auto alloc_info = *(mem_space.begin());
    if (!alloc_info.free)
        throw std::logic_error("Root object is already allocated in the memory space");

    return Create<T>(mem_space, std::forward<Arg>(args)...);
}

template <typename T>
[[nodiscard]] auto SME_EXPORT GetRoot(MemorySpace& mem_space) -> T&
{
    auto alloc_info = *(mem_space.begin());
    if (alloc_info.free)
        throw std::logic_error("Root object is not allocated in the memory space");
    if (alloc_info.size < sizeof(T))
        throw std::logic_error("Root object size is not matched");

    return *std::launder(reinterpret_cast<T*>(alloc_info.data));
}

template <typename T>
[[nodiscard]] auto SME_EXPORT GetRoot(const MemorySpace& mem_space) -> const T&
{
    auto alloc_info = *(mem_space.begin());
    if (alloc_info.free)
        throw std::logic_error("Root object is not allocated in the memory space");
    if (alloc_info.size < sizeof(T))
        throw std::logic_error("Root object size is not matched");

    return *std::launder(reinterpret_cast<const T*>(alloc_info.data));
}

}  // namespace msp
}  // namespace sme

#endif  // SME_MSP_ROOT_OBJ_H
