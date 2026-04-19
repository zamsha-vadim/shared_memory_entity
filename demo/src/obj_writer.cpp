#include <fcntl.h>
#include <sys/stat.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <mutex>

#include <sme/futex.h>
#include <sme/mapped_obj.h>
#include <sme/mdm/unique_ptr.h>
#include <sme/mem_space.h>
#include <sme/msp/root_obj.h>
#include <sme/shm.h>

#include "ref_layout.h"
#include "simple_obj.h"


/* NEEDED: USE after
std::cout << mem_space->GetCapacity() << std::endl;

for (int i = 0; ; i++) {
    sme::Pointer<sme::MemoryDomain> domain =
        sme::CreateMemoryDomain(*mem_space, 1024 * 1);

    std::cout << ((i + 1) * 1024)  << std::endl;
    for (const auto& seg_info : domain->GetAllSegmentInfo()) {
        std::cout << "address: " << seg_info.address << " - "
                  << (const void*)(reinterpret_cast<const char*>(seg_info.address) +
                                   seg_info.size)
                  << std::endl;
        std::cout << "size:    " << seg_info.size << std::endl;
    }
}
*/

auto CreateTimeDuration(int value, sme::MemoryDomain& mem_domain)
    -> sme::mdm::UniquePtr<IntObject>
{
    static const std::array<const char*, 4> measures = {"msec", "sec", "min", "hour"};

    auto iv = sme::make_unique<IntObject>(mem_domain, "time duration", value, measures,
                                          mem_domain);
    iv->SetMeasure(measures[1]);

    return iv;
}

template <typename ValueT>
auto Send(ReferenceLayout& rlay, sme::mdm::UniquePtr<SimpleObject<ValueT>>& obj) -> bool
{
    std::cout << "Sending..." << std::flush;

    std::unique_lock<sme::Mutex> mutex_lock{rlay.mutex};

    rlay.object_type =
        sme::mdm::MakeStringUnique(*rlay.object_domain, typeid(*obj).name());

    sme::Pointer<void> ptr = obj.release();
    rlay.object = ptr;

    rlay.result_flag = 1;

    mutex_lock.unlock();
    std::cout << "Done" << std::endl;
    std::cout << "Notifying readers for readiness..." << std::flush;

    sme::FutexWake(rlay.result_flag);

    std::cout << "Done" << std::endl;

    return true;
}

void WaitProcessed(ReferenceLayout& rlay)
{
    std::cout << "Wait for message processed... " << std::flush;

    while (rlay.result_flag != 0) {
        if (sme::FutexWait(rlay.result_flag, 1, std::chrono::seconds(6)) !=
            sme::FutexResult::kCompleted) {
            std::cout << "Failed, no readers" << std::endl;
            return;
        }
    }

    std::cout << "Successful" << std::endl;
}

int main()
{
    try {
        const char* shm_name = "/sme_obj_mem";
        constexpr size_t kSomeSpaceSize{1024UL * 128};

        sme::SharedMemoryFile smf{shm_name, O_CREAT | O_RDWR};

        auto initialized = (smf.GetSize() != 0);
        if (!initialized) {
            smf.SetSize(kSomeSpaceSize);
        }

        sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

        sme::MemorySpace* mem_space{};
        ReferenceLayout* rlay{};
        sme::Pointer<sme::MemoryDomain> obj_domain;

        if (initialized) {
            mem_space = &sme::GetMemorySpace(mem_map);
            rlay = &sme::msp::GetRoot<ReferenceLayout>(*mem_space);

            initialized = (rlay->check_id1 == ReferenceLayout::kCheckValidId &&
                           rlay->check_id2 == ~ReferenceLayout::kCheckValidId);

            if (initialized)
                obj_domain = rlay->object_domain;
        }

        if (!initialized) {
            mem_space = sme::ConstructMemorySpace(mem_map);
            rlay = sme::msp::CreateRoot<ReferenceLayout>(*mem_space);

            obj_domain =
                sme::CreateMemoryDomain(*mem_space, mem_space->GetCapacity() / 2);

            rlay->object_domain = obj_domain;
        }

        auto td_obj = CreateTimeDuration(123, *obj_domain);
        Print(*td_obj);

        Send(*rlay, td_obj);
        WaitProcessed(*rlay);

        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "*** Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
