#include <fcntl.h>
#include <sys/stat.h>

#include <array>
#include <cstdlib>
#include <iostream>

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
    std::unique_lock<sme::Mutex> ml{ref_layout.mutex};

//    if (ref_layout.simple_object == nullptr)
//        return false;

    ref_layout.simple_object_type =
        sme::mdm::MakeStringUnique(*ref_layout.simple_object_domain, typeid(*obj).name());

    sme::Pointer<void> ptr = obj.release();
    ref_layout.simple_object = ptr;

    ref_layout.cond_var.NotifyOne();

    return true;
}

int main()
{
    const char* shm_name = "/sme_demo_mem";
    sme::SharedMemoryFile smf{shm_name, O_CREAT | O_RDWR};
    smf.SetSize(kSomeSpaceSize);

    sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestAsShared);
    sme::MemorySpace* mem_space = sme::ConstructMemorySpace(mem_map);

    ReferenceLayout* ref_layout = sme::msp::CreateRoot<ReferenceLayout>(*mem_space);

    sme::Pointer<sme::MemoryDomain> simple_obj_domain =
        sme::CreateMemoryDomain(*mem_space, mem_space->GetCapacity() / 2);
    ref_layout->simple_object_domain = simple_obj_domain;

/*
    sme::Pointer<sme::MemoryDomain> composite_obj_domain =
        sme::CreateMemoryDomain(*mem_space, mem_space->GetCapacity() / 2 - 1000);
    ref_layout->composite_object_domain = composite_obj_domain;
*/

    auto td_obj = CreateTimeDuration(123, *simple_obj_domain);
    Print(*td_obj);

    Send(*ref_layout, td_obj);

//    auto month_obj = CreateMonth("may", *simple_obj_domain);
//    Print(*month_obj);

    return EXIT_SUCCESS;
}
