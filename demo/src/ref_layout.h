#ifndef REF_LAYOUT_H
#define REF_LAYOUT_H

#include <sme/cond_var.h>
#include <sme/mem_domain.h>
#include <sme/mem_ptr.h>
#include <sme/mutex.h>

#include "simple_obj.h"

constexpr uint64_t kCheckValidId{15042026UL};

struct ReferenceLayout {
    uint64_t check_id1{kCheckValidId};

    sme::Mutex mutex{sme::InterprocessVisibility::kShared};
    sme::ConditionVariable cond_var{sme::InterprocessVisibility::kShared};

    sme::Pointer<sme::MemoryDomain> simple_object_domain;
    sme::Pointer<void*> simple_object;
    sme::mdm::UniquePtr<sme::mdm::string> simple_object_type;

    uint64_t check_id2{~kCheckValidId};

//    sme::Pointer<sme::MemoryDomain> composite_object_domain;
};

#endif // REF_LAYOUT_H
