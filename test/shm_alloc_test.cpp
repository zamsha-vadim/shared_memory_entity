#include <fcntl.h>
#include <sys/types.h>

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string_view>
#include <type_traits>

#include "sme/msp/deque.h"
#include "sme/msp/forward_list.h"
#include "sme/msp/list.h"
#include "sme/msp/root_obj.h"
#include "sme/msp/set.h"
#include "sme/msp/string.h"
#include "sme/msp/vector.h"
#include "sme/shm_obj.h"

constexpr std::string_view kShareMemoryName{"/shm_test"};
constexpr size_t kSharedMemorySize{1024U * 1024U * 1};

constexpr auto kCreateFlags = O_CREAT | O_RDWR;
constexpr auto kOpenFlags = O_RDWR;
constexpr auto kUserRwRights = S_IRUSR | S_IWUSR;

[[maybe_unused]] void PrintAllocations(const sme::MemorySpace& mem_space)
{
    for (const auto& alloc : mem_space) {
        std::cout << '[' << alloc.block << "-"
                  << static_cast<const void*>(static_cast<const char*>(alloc.block) +
                                              alloc.size)
                  << ", " << alloc.position << ", " << alloc.size << '('
                  << (alloc.size - sme::kMemorySpaceBlockHeaderSize) << "), "
                  << (alloc.free ? "free" : "used") << "]\n";
    }
    std::cout << std::endl;
}

sme::msp::string CreateString(sme::msp::MemorySpaceAllocator<sme::msp::string>& str_alloc,
                              int id)
{
    std::string s;

    auto count = (id % 10);
    for (auto i = 0u; i < (5u + count); i++) {
        s += std::to_string(id + i);
        s += '_';
    }
    s += '!';

    sme::msp::string::allocator_type char_alloc{str_alloc};

    return sme::msp::string(s, char_alloc);
}

template <typename Allocator>
double CreateDouble(const Allocator&, int id)
{
    return id * 10;
}

template <typename Container, typename ValueCreatorType>
void SetValues(Container& container, ValueCreatorType value_creator)
{
    auto allocator = container.get_allocator();

    for (auto i = 1; i <= 1000; i++) {
        auto value = value_creator(allocator, i);

        container.insert(container.begin(), std::move(value));
    }
}

template <typename Container>
void ShowValues(const std::string& title_prefix, const Container& container)
{
    std::cout << title_prefix << " size: " << container.size() << std::endl;
    for (const auto& value : container)
        std::cout << title_prefix << ": " << value << std::endl;
    std::cout << std::endl;
}

template <typename Container>
bool Compare(const Container& lhs, const Container& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    auto lhs_iter = lhs.cbegin();
    auto rhs_iter = rhs.cbegin();

    while (lhs_iter != lhs.cend()) {
        if (*lhs_iter != *rhs_iter)
            return false;
        ++lhs_iter;
        ++rhs_iter;
    }

    return true;
}

template <typename Container>
auto Read(sme::MemorySpace& mem_space) -> sme::Pointer<Container>
{
    auto container = sme::msp::GetRootObject<Container>(mem_space);
    return container;
}

template <typename Container>
void Read()
{
    sme::SharedMemoryFile smf{kShareMemoryName, kOpenFlags};
    auto mapped_mem = smf.MapMemory();
    auto* mem_space = sme::GetSharedObject<sme::MemorySpace>(mapped_mem, 9);

    Read<Container>(*mem_space);
}

template <typename Container, typename ValueCreator>
auto Write(sme::MemorySpace& mem_space, ValueCreator value_creator)
    -> sme::Pointer<Container>
{
    typename Container::allocator_type item_alloc{mem_space};

    sme::Pointer<Container> container =
        sme::msp::CreateRootObject<Container>(mem_space, item_alloc);
    SetValues(*container, value_creator);

    return container;
}

template <typename Container, typename ValueCreator>
void TestSharedMemory(ValueCreator value_creator)
{
    bool is_valid{};

    try {
        sme::SharedMemoryFile smf1{kShareMemoryName, kCreateFlags, kUserRwRights,
                                   kSharedMemorySize};
        auto mapped_mem1 = smf1.MapMemory();
        auto* mem_space1 = sme::CreateSharedObject<sme::MemorySpace>(mapped_mem1, 0);
        auto container1 = Write<Container>(*mem_space1, value_creator);

        sme::SharedMemoryFile smf2{kShareMemoryName, kOpenFlags};
        auto mapped_mem2 = smf2.MapMemory();
        auto& mem_space2 = sme::GetSharedObject<sme::MemorySpace>(mapped_mem2, 0);
        auto container2 = Read<Container>(mem_space2);

        is_valid = Compare<Container>(*container1, *container2);
        if (is_valid) {
            sme::SharedMemoryFile smf3{kShareMemoryName, kOpenFlags};
            auto mapped_mem3 = smf3.MapMemory();
            auto& mem_space3 = sme::GetSharedObject<sme::MemorySpace>(mapped_mem3, 0);
            auto container3 = Read<Container>(mem_space3);

            mapped_mem1.Unmap();

            is_valid = Compare(*container2, *container3);
        }

        sme::SharedMemoryFile::UnlinkIfExists(kShareMemoryName);

    } catch (...) {
        sme::SharedMemoryFile::UnlinkIfExists(kShareMemoryName);

        throw;
    }

    if (!is_valid) {
        std::cerr << "Shared memory test FAILED" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main()
{
    try {
        TestSharedMemory<sme::msp::vector<double>>(
            &CreateDouble<sme::msp::vector<double>::allocator_type>);
        TestSharedMemory<sme::msp::vector<sme::msp::string>>(&CreateString);

        TestSharedMemory<sme::msp::deque<double>>(
            &CreateDouble<sme::msp::deque<double>::allocator_type>);
        TestSharedMemory<sme::msp::deque<sme::msp::string>>(&CreateString);

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;

    } catch (...) {
        std::cerr << "Unknown error issued" << std::endl;
        return EXIT_FAILURE;
    }

    std::cerr << "PASSED" << std::endl;
    return EXIT_SUCCESS;
}
