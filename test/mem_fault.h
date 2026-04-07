#ifndef SME_TEST_MEM_FAULT_H
#define SME_TEST_MEM_FAULT_H

namespace sme {

class MemoryFaultCatcher {
   public:
    struct AddressRange {
        const void* begin{};
        const void* end{};
    };

   public:
    MemoryFaultCatcher(const void* begin_addr, const void* end_addr);

    MemoryFaultCatcher(const MemoryFaultCatcher&) = delete;
    MemoryFaultCatcher(MemoryFaultCatcher&&) = delete;
    MemoryFaultCatcher& operator=(const MemoryFaultCatcher&) = delete;
    MemoryFaultCatcher& operator=(MemoryFaultCatcher&&) = delete;

    ~MemoryFaultCatcher();
};

}  // namespace sme

#endif // SME_TEST_MEM_FAULT_H
