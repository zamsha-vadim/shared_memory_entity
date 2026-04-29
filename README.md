### Project Goals

The primary objective of this project is to facilitate high-performance data exchange between
applications via **Shared Memory**. This is achieved using standard C++ classes that can
encapsulate C-strings, STL containers, and other dynamically allocated C++ objects.

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
