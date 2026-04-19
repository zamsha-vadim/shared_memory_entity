#include <chrono>
#include <iostream>

#include <sme/shm.h>
#include "sme/msg_channel.h"

#include "entity.h"

auto GenerateEntityId() -> uint64_t
{
    const auto now = std::chrono::system_clock::now();
    return now.time_since_epoch().count();
}

int main()
{
    try {
        const char* shm_name = "/sme_channel_mem";
        constexpr size_t kSomeSpaceSize{1024UL * 128};

        sme::SharedMemoryFile smf{shm_name, O_CREAT | O_RDWR};

        auto initialized = (smf.GetSize() != 0);
        if (!initialized) {
            smf.SetSize(kSomeSpaceSize);
        }

        sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

        sme::MessageChannel channel{
            std::move(mem_map), !initialized ? sme::MessageChannel::InitialState::kCreate
                                             : sme::MessageChannel::InitialState::kOpen};

        sme::MessageWriter& writer = channel.GetWriter();

        sme::IntrusivePtr<sme::Message> msg = writer.CreateMessage();

        sme::MemoryDomain& mem_domain = msg->GetAllocationDomain();
        sme::AllocationContext alloc_ctx{mem_domain};

        auto* entity = sme::Create<Entity>(mem_domain, GenerateEntityId(), "Hello world");
        auto& props = entity->GetProperties();

        for (auto i = 0u; i < 10; i++) {
            std::string name = "p" + std::to_string(i);

            Entity::Property prop{.name = name.c_str(), .value = GenerateEntityId() + i};
            props.push_back(std::move(prop));
        }


        const auto kTypeId{0x1234};
        msg->SetRootObject(entity, kTypeId);

        std::cout << "Output message: id=" << entity->GetId()
                  << ", name=" << entity->GetName() << std::endl;
        for (const auto& prop : entity->GetProperties()) {
            std::cout << "  property: name=" << prop.name << ", value=" << prop.value
                      << std::endl;
        }

        sme::QueueResult res = writer.Commit(msg);

        if (res == sme::QueueResult::kSuccessful) {
            std::cerr << "Message sent successfully" << std::endl;
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Message sending error" << std::endl;
            return EXIT_FAILURE;
        }

    } catch (const std::exception& ex) {
        std::cerr << "*** Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
