#include "shm_test_util.h"

#include <sys/stat.h>
#include <unistd.h>

#include <sstream>
#include <thread>

auto GetUniqueName() -> std::string
{
    static uint64_t id{0};

    std::ostringstream ss;
    ss << "/shm_test_name_" << getpid() << '-' << std::this_thread::get_id() << '-'
       << (++id);
    return ss.str();
}

auto CreateTestSharedMemoryFile(const std::string& name, size_t size)
    -> sme::SharedMemoryFile
{
    sme::SharedMemoryFile::UnlinkIfExists(name);

    return sme::SharedMemoryFile{name, kShmCreateFlags, kShmUserRwRights, size};
}

auto OpenTestSharedMemoryFile(const std::string& name) -> sme::SharedMemoryFile
{
    return sme::SharedMemoryFile{name, kShmOpenFlags, kShmUserRwRights};
}

void FillMappedMemory(sme::MemoryMap& mem_map, char value, size_t cycle_count)
{
    char* area = static_cast<char*>(mem_map.GetAddress());

    for (auto i = 0U; i < cycle_count; i++)
        area[i] = value;
}

