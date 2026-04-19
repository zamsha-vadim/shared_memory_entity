#ifndef REF_LAYOUT_H
#define REF_LAYOUT_H

#include <atomic>

#include <sme/mem_domain.h>
#include <sme/mem_ptr.h>
#include <sme/mutex.h>

#include "simple_obj.h"

struct ReferenceLayout {
    static constexpr uint64_t kCheckValidId{15042026UL};
    uint64_t check_id1{kCheckValidId};

    sme::Mutex mutex{sme::InterprocessVisibility::kShared};
    std::atomic<uint32_t> result_flag;

    sme::Pointer<sme::MemoryDomain> object_domain;
    sme::Pointer<void*> object;
    sme::mdm::UniquePtr<sme::mdm::string> object_type;

    uint64_t check_id2{~kCheckValidId};
};

#endif // REF_LAYOUT_H
