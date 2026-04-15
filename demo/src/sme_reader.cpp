#include <fcntl.h>
#include <sys/stat.h>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include <sme/mapped_obj.h>
#include <sme/mdm/unique_ptr.h>
#include <sme/mem_space.h>
#include <sme/msp/root_obj.h>
#include <sme/shm.h>

#include "ref_layout.h"
#include "simple_obj.h"

int main()
{
    const char* shm_name = "/sme_demo_mem";
    sme::SharedMemoryFile smf{shm_name, O_RDWR};

    sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestAsShared);
    sme::MemorySpace& mem_space = sme::GetObject<sme::MemorySpace>(mem_map, 0);

    ReferenceLayout& ref_layout = sme::msp::GetRoot<ReferenceLayout>(mem_space);

    std::unique_lock<sme::Mutex> mutex_lock{ref_layout.mutex};

    assert(ref_layout.check_id1 == kCheckValidId);
    assert(ref_layout.check_id2 == ~kCheckValidId);

    while (ref_layout.simple_object == nullptr) {
        if (!ref_layout.cond_var.WaitFor(mutex_lock, std::chrono::seconds(60))) {
            return EXIT_FAILURE;
        }
    }

    assert(ref_layout.simple_object != nullptr);

    std::cout << *(ref_layout.simple_object_type) << std::endl;

    const auto& type_id = *(ref_layout.simple_object_type);

    if (type_id == typeid(IntObject).name()) {
        sme::Pointer<IntObject> obj =
            reinterpret_cast<IntObject*>(ref_layout.simple_object.GetAddress());
        Print(*obj);

        sme::Delete(*(ref_layout.simple_object_domain), obj);

        ref_layout.simple_object = nullptr;
        ref_layout.simple_object_type.release();

        ref_layout.cond_var.NotifyOne();
    }

    return EXIT_SUCCESS;
}
