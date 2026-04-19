#include <iostream>

#include <sme/shm.h>
#include "sme/msg_channel.h"

#include "entity.h"

int main()
{
    try {
        const char* shm_name = "/sme_channel_mem";

        sme::SharedMemoryFile smf{shm_name, O_RDWR};
        sme::MemoryMap mem_map = smf.MapMemory(sme::kAllMemoryMapRequestForShared);

        sme::MessageChannel channel{std::move(mem_map),
                                    sme::MessageChannel::InitialState::kOpen};
        sme::MessageReader& reader = channel.GetReader();

        sme::QueueResult res{};
        sme::IntrusivePtr<sme::Message> msg;

        std::tie(res, msg) = reader.Read();
        if (res != sme::QueueResult::kSuccessful) {
            std::cerr << "Message reading error" << std::endl;
            return EXIT_FAILURE;
        }

        sme::Pointer<Entity> entity = msg->GetRootObjectDefintion().object;

        std::cout << "Input message: id=" << entity->GetId()
                  << ", name=" << entity->GetName() << std::endl;
        for (const auto& prop : entity->GetProperties()) {
            std::cout << "  property: name=" << prop.name << ", value=" << prop.value
                      << std::endl;
        }

        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "*** Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
