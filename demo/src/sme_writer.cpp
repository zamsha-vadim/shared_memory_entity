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

constexpr size_t kSomeSpaceSize{1024UL * 128};
std::array<char, kSomeSpaceSize> some_memory{};

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

    auto iv = sme::MakeUnique<IntObject>(mem_domain, "time duration", value, measures,
                                         mem_domain);
    iv->SetMeasure(measures[1]);

    return iv;
}

auto CreateMonth(const char* value, sme::MemoryDomain& mem_domain)
    -> sme::mdm::UniquePtr<StringObject>
{
    static const std::array<const char*, 3> dayAmount = {"30", "31", "28"};

    sme::mdm::string sval{value, sme::mdm::ItemAllocator<sme::mdm::string>(mem_domain)};

    auto sv =
        sme::MakeUnique<StringObject>(mem_domain, "month", sval, dayAmount, mem_domain);
    sv->SetMeasure(dayAmount[0]);

    return sv;
}

template <typename ValueT>
auto Send(ReferenceLayout& ref_layout, sme::mdm::UniquePtr<SimpleObject<ValueT>>& obj)
    -> bool
{
    std::cout << "Begin sending" << std::flush;

    std::unique_lock<sme::Mutex> mutex_lock{ref_layout.mutex};

    std::cout << "..." << std::flush;

    /*
    if (ref_layout.simple_object != nullptr) {
        return false;
    }
    */

    ref_layout.simple_object_type =
        sme::mdm::MakeStringUnique(*ref_layout.simple_object_domain, typeid(*obj).name());

    sme::Pointer<void> ptr = obj.release();
    ref_layout.simple_object = ptr;

    ref_layout.result_flag = 1;

    mutex_lock.unlock();
    std::cout << "notifying..." << std::flush;

    sme::FutexWake(ref_layout.result_flag);

    std::cout << "Done" << std::endl;

    return true;
}

void WaitProcessed(ReferenceLayout& ref_layout)
{
    std::cout << "Wait for message processed... " << std::flush;

    while (ref_layout.result_flag != 0) {
        if (sme::FutexWait(ref_layout.result_flag, 1, std::chrono::seconds(6)) !=
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
        const char* shm_name = "/sme_demo_mem";
        sme::SharedMemoryFile smf{shm_name, O_CREAT | O_RDWR};

        auto initialized = (smf.GetSize() != 0);
        if (!initialized) {
            smf.SetSize(kSomeSpaceSize);
        }

        sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestAsShared);

        sme::MemorySpace* mem_space{};
        ReferenceLayout* ref_layout{};
        sme::Pointer<sme::MemoryDomain> simple_obj_domain;

        if (initialized) {
            mem_space = &sme::GetMemorySpace(mem_map);
            ref_layout = &sme::msp::GetRoot<ReferenceLayout>(*mem_space);

            initialized = (ref_layout->check_id1 == ReferenceLayout::kCheckValidId &&
                           ref_layout->check_id2 == ~ReferenceLayout::kCheckValidId);

            if (initialized)
                simple_obj_domain = ref_layout->simple_object_domain;
        }

        if (!initialized) {
            mem_space = sme::ConstructMemorySpace(mem_map);
            ref_layout = sme::msp::CreateRoot<ReferenceLayout>(*mem_space);

            simple_obj_domain =
                sme::CreateMemoryDomain(*mem_space, mem_space->GetCapacity() / 2);

            ref_layout->simple_object_domain = simple_obj_domain;
        }


        auto td_obj = CreateTimeDuration(123, *simple_obj_domain);
        Print(*td_obj);

        Send(*ref_layout, td_obj);
        WaitProcessed(*ref_layout);

        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "*** Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
