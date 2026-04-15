#include <fcntl.h>
#include <sys/stat.h>

#include <array>
#include <cstdlib>
#include <iostream>

#include <sme/mapped_obj.h>
#include <sme/mem_space.h>
#include <sme/msp/root_obj.h>
#include <sme/shm.h>

#include "ref_layout.h"
#include "simple_obj.h"

constexpr size_t kSomeSpaceSize{1024UL * 128};
std::array<char, kSomeSpaceSize> some_memory{};

/*
[[maybe_unused]] auto CreateTestMemorySpace(size_t size = some_memory.size())
    -> sme::MemorySpace
{
    assert(size <= some_memory.size());
    some_memory.fill(0);
    return sme::MemorySpace{sme::Pointer<void>{some_memory.data()}, size};
}
*/
/*
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

template <typename T>
void Print(const T& obj)
{
    std::cout << "name:    " << obj.GetName() << std::endl
              << "value    " << obj.GetValue() << std::endl
              << "measure: " << obj.GetMeasure() << std::endl;

    const auto& accept_measures = obj.GetAcceptableMeasures();
    if (!accept_measures.empty()) {
        std::cout << "acceptable measures:" << std::endl;
        for (const auto& measure : accept_measures)
            std::cout << "         " << measure << std::endl;
    }
}

auto CreateSimpleObject(sme::MemoryDomain& mem_domain) //-> SimpleObject
{
    std::array<const char*, 4> measures = {"msec", "sec", "min", "hour"};

    SimpleObject<int> iv{"Time duration", measures, mem_domain};
    iv.SetValue(123);
    iv.SetMeasure(measures[1]);

//    return iv;
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

    sme::Pointer<sme::MemoryDomain> composite_obj_domain =
        sme::CreateMemoryDomain(*mem_space, mem_space->GetCapacity() / 2 - 1000);

    ref_layout->simple_object_domain = simple_obj_domain;
    ref_layout->composite_object_domain = composite_obj_domain;

    //CreateSimpleObject(*simple_object_domain);

    // std::cout << mem_space.GetCapacity() << std::endl;

    return EXIT_SUCCESS;
}
