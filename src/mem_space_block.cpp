#include "sme/internal/mem_space_block.h"

namespace sme {

auto MemorySpaceBlock::GetDataSize() const noexcept -> size_t
{
    return (size - kMemorySpaceBlockHeaderSize);
}

}  // namespace sme
