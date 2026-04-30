### Project Goals

The primary objective of this project is to facilitate high-performance data exchange between
applications via **Shared Memory**. This is achieved using standard C++ classes that can
encapsulate C-strings, STL containers, and other dynamically allocated C++ objects.

NOTE: Only Linux implementation

#### Key Features and Constraints:

*   **Zero-Copy Communication:** Data exchange is performed through shared memory segments,
    bypassing traditional serialization overhead.
*   **Minimalist Class Requirements:** The only constraint on data classes is the **absence of
    virtual methods** (to prevent issues with vtable pointer consistency across different process
    address spaces).
*   **Relocatable Memory Management:** Since shared memory segments can be mapped to different
    virtual addresses in each process, standard `malloc` and `new` are inapplicable. The project
    implements a **custom Memory Manager** tailored for cross-process heap allocation.
*   **Optimized Allocation Logic:** The memory manager is designed for simplicity. It follows a
    "single-allocator, single-reclaimer" pattern where memory is typically allocated exclusively
    in one process, then ownership is transferred to another process for exclusive use and
    subsequent deallocation.
*   **Synchronization & Signaling:** The library provides mechanisms for synchronized memory access
    and inter-process notifications (signaling data readiness).
*   **Inter-Process Message Queue:** A robust **Lock-Free Message Queue** is provided for passing
    complex C++ objects between processes.
*   **STL Compatibility:** Specialized **STL-compliant allocators** are included, allowing the use
    of standard containers (e.g., `std::string`, `std::vector`) within shared memory.
*   **Utility Toolset:** A set of helper functions is provided to simplify shared memory life-cycle
    management and pointer arithmetic.

This suite of classes provides a comprehensive solution for building low-latency IPC (Inter-Process
Communication) systems in C++.
### Core classes:
* **sme::SharedMemoryFile** — creates/opens a shared memory object.
* **sme::MemoryMap** — maps shared memory into the virtual address space of the process.
* **sme::MemorySpace** — allocates memory blocks that can serve as local areas.
* **sme::Pointer<Type>** — one of the fundamental classes implementing pointers in memory via 
  relative offsets.
* **sme::MemoryDomain** — local memory allocator in a memory block allocated via 
  sme::MemorySpace calls. It is used by STL memory allocators.
* **sme::AllocationContext** — simplifies the use of memory allocators for strings and 
  containers from the Standard Library.
* **sme::MessageChannel** — provides a channel for exchanging messages between processes. 
  Uses an internal implementation of a lock-free message queue. Messages are allocated via 
  sme::MemoryDomain calls in a separate memory area allocated in sme::MemorySpace.
* **sme::Message** — implements messages for sme::MessageChannel.
* **sme::IntrusivePtr<sme::Message>** — pointer to messages for reading/writing them in 
  sme::MessageChannel.
* **sme::MessageWriter** — writes messages to sme::MessageChannel.
* **sme::MessageReader** — reads messages from sme::MessageChannel.

Data exchange between processes can be performed in two ways: through independent memory areas 
where objects are located, or through a message channel with arbitrary data.

### Usage examples are provided in the `demo` directory:
* The first method is demonstrated in `demo/src/obj_writer.cpp` and `demo/src/obj_reader.cpp`.
* The second method is in `demo/src/channel_writer.cpp` and `demo/src/channel_reader.cpp`.

For implementation details, you can also refer to the tests in the `tests` directory.

---
*Detailed documentation on usage is being written...*
