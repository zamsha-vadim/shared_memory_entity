#ifndef REF_LAYOUT_H
#define REF_LAYOUT_H

#include <sme/alloc_util.h>
#include <sme/mem_domain.h>
#include <sme/mem_ptr.h>
#include <sme/mutex.h>

#include "simple_obj.h"

struct ReferenceLayout {
    sme::Mutex mutex{sme::InterprocessVisibility::kShared};

    sme::Pointer<sme::MemoryDomain> simple_object_domain;
    sme::Pointer<IntObject> simple_object;
    //sme::UniquePtr<IntObject, sme::MemoryDomain> simple_object;

    sme::Pointer<sme::MemoryDomain> composite_object_domain;
};

#endif // REF_LAYOUT_H
