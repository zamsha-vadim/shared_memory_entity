#include <array>
#include <iostream>

#include "sme/mem_space.h"

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

int main() {
  sme::Pointer<void> mem_ptr = some_memory.data();

  sme::MemorySpace mem_space(mem_ptr, some_memory.size());

  std::cout << mem_space.GetCapacity() << std::endl;

  return 0;
}
