#ifndef SME_TEST_SHM_TEST_UTIL_H
#define SME_TEST_SHM_TEST_UTIL_H

#include <fcntl.h>
#include <sys/types.h>

#include <string>

#include "sme/shm.h"

constexpr auto kShmCreateFlags = O_CREAT | O_RDWR;
constexpr auto kShmOpenFlags = O_RDWR;
constexpr auto kShmUserRwRights = S_IRUSR | S_IWUSR;

auto GetUniqueName() -> std::string;

auto CreateTestSharedMemoryFile(const std::string& name, size_t size)
    -> sme::SharedMemoryFile;

auto OpenTestSharedMemoryFile(const std::string& name) -> sme::SharedMemoryFile;

class SharedMemoryFileDeleter {
   public:
    SharedMemoryFileDeleter(const sme::SharedMemoryFile& smf) : path_{smf.GetPath()} {}
    ~SharedMemoryFileDeleter() { sme::SharedMemoryFile::UnlinkIfExists(path_); }

   private:
    std::string path_;
};

void FillMappedMemory(sme::MemoryMap& mem_map, char value, size_t cycle_count);

#endif  // SME_TEST_SHM_TEST_UTIL_H
