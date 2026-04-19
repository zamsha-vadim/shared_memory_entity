#include <fcntl.h>
#include <sys/stat.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <new>

#include <sme/futex.h>
#include <sme/mapped_obj.h>
#include <sme/mdm/unique_ptr.h>
#include <sme/mem_space.h>
#include <sme/msp/root_obj.h>
#include <sme/shm.h>

#include "ref_layout.h"
#include "simple_obj.h"

int main()
{
    try {
        const char* shm_name = "/sme_demo_mem";
        sme::SharedMemoryFile smf{shm_name, O_RDWR};

        sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);
        sme::MemorySpace& mem_space = sme::GetObject<sme::MemorySpace>(mem_map, 0);

        ReferenceLayout& rlay = sme::msp::GetRoot<ReferenceLayout>(mem_space);

        std::unique_lock<sme::Mutex> mutex_lock{rlay.mutex};

        assert(rlay.check_id1 == ReferenceLayout::kCheckValidId);
        assert(rlay.check_id2 == ~ReferenceLayout::kCheckValidId);

        while (rlay.object == nullptr) {
            mutex_lock.unlock();

            if (sme::FutexWait(rlay.result_flag, 0, std::chrono::seconds(60)) ==
                sme::FutexResult::kTimeout)
                return EXIT_FAILURE;

            mutex_lock.lock();
        }

        assert(rlay.object != nullptr);

        std::cout << *(rlay.object_type) << std::endl;

        const auto& type_id = *(rlay.object_type);

        if (type_id == typeid(IntObject).name()) {
            sme::Pointer<IntObject> obj =
                std::launder(reinterpret_cast<IntObject*>(rlay.object.GetAddress()));
            Print(*obj);

            sme::Delete(*(rlay.object_domain), obj);

            rlay.object = nullptr;
            rlay.object_type.release();

            rlay.result_flag = 0;

            mutex_lock.unlock();

            sme::FutexWake(rlay.result_flag);
        }

        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "*** Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
