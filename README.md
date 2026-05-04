# SME (Shared Memory Entity)

### Project Goals

The primary objective of this project is to facilitate high-performance data exchange between
applications via **Shared Memory**. This is achieved using standard C++ classes that can
encapsulate C-strings, STL containers, and other dynamically allocated C++ objects.

* **Operating System**: Linux (Tested on Ubuntu 22.04; support for other distributions is planned).
* **Compiler**: GCC 11+ or Clang 14+ (C++17 support required).

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
    "single-allocator, multiple-reclaimer" pattern where memory is typically allocated exclusively
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


### Implementation Motivation

When using shared memory between processes, there is often a need to exchange data represented by 
natural C++ class objects. These are frequently Data Transfer Objects (DTOs) rather than complex 
business logic entities. Their task is to transfer data from one application to another. Despite 
their simple logical structure, they may contain large amounts of dynamically allocated data.

However, constraints often arise due to class members represented by data pointers. When memory is 
allocated dynamically within shared memory, such pointers become invalid in the memory space of 
another process because the shared memory is mapped to different virtual addresses. Another 
problem is the requirement to know the exact size of dynamically allocated data and manage its 
changes during subsequent allocations. This is typically solved by over-allocating memory, which 
is suboptimal if data sizes vary significantly and shared memory is a limited resource.

Consequently, developers often resort to serializing these C++ objects into binary or text 
representations. This consumes additional time and resources on both the sending and receiving 
sides. In addition to data transfer, process synchronization issues arise. Standard C++ 
thread-synchronization primitives like `std::mutex` or `std::condition_variable` are unsuitable 
because they carry the context of an individual process. Thus, they cannot be used correctly for 
inter-process interaction, even if placed within shared memory.

This library enables the exchange of full-fledged C++ objects, allowing for dynamic memory 
allocation for internal data, as well as safe access to these objects through inter-process 
synchronization mechanisms. Additionally, a message queue is provided, enabling the exchange 
of data in the form of arbitrary C++ objects.

### Data Transfer Object (DTO) Example

This class represents the concept of an "entity" with its name and a list of named properties 
of type uint64_t. Before conversion, it looks as follows:

```cpp
class Entity {
    public:
     struct Property {
         std::string name;
         uint64_t value;
     };

   public:
    Entity(uint64_t id, const char* name);

    auto GetId() const noexcept -> uint64_t;

    auto GetName() const noexcept -> const std::string&;
    void SetName(const char* name);

    auto GetProperties() const noexcept -> const std::deque<Property>&;
    auto GetProperties() noexcept -> std::deque<Property>&;

   private:
    uint64_t id_{};
    std::string name_;
    std::deque<Property> props_;
};
```

After conversion for use in the shared memory context:

```cpp
class Entity {
    public:
     struct Property {
         sme::mdm::string name;
         uint64_t value;
     };

   public:
    Entity(uint64_t id, const char* name);

    auto GetId() const noexcept -> uint64_t;

    auto GetName() const noexcept -> const sme::mdm::string&;
    void SetName(const char* name);

    auto GetProperties() const noexcept -> const sme::mdm::deque<Property>&;
    auto GetProperties() noexcept -> sme::mdm::deque<Property>&;

   private:
    uint64_t id_{};
    sme::mdm::string name_;
    sme::mdm::deque<Property> props_;
};
```

In effect, the C++ namespace was changed from `std::` to `sme::mdm`. However, these are not 
entirely new classes; they are the same C++ STL strings and containers. These are simply 
`typedef`/`using` declarations that account for the specifics of shared memory allocation via 
STL-compliant allocators. Initialization and usage remain almost identical to the standard 
patterns of these classes.

### Brief Description of Core Classes

This suite of classes provides a comprehensive solution for building low-latency IPC systems in C++.

* **sme::SharedMemoryFile** — creates/opens a shared memory object.
* **sme::MemoryMap** — maps shared memory into the virtual address space of the process.
* **sme::MemorySpace** — allocates memory blocks that can serve as local areas.
* **sme::Pointer<Type>** — a fundamental class implementing pointers in memory via relative 
  offsets.
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

All the classes listed above, with the exception of `sme::SharedMemoryFile` and `sme::MemoryMap`, 
can also be used for inter-thread communication within a single process.

### Class Usage

Usage begins with creating or opening a shared memory file using `sme::SharedMemoryFile` methods.
Mapping this memory into the address space is performed via `sme::MemoryMap` methods. These 
classes are straightforward in their usage, so we will not focus on them in detail here.

### Class `sme::Pointer<Type>`

An essential class for working with pointers in shared memory is `sme::Pointer<Type>`. It 
implements the "fancy pointer" concept. Since shared memory regions are projected onto different 
virtual memory addresses across different processes, standard pointers cannot be used. The 
`sme::Pointer` class is used transparently in place of regular pointers and is returned by all 
memory allocation functions within the shared memory region.

### Class `sme::MemorySpace`

This class attaches to a large memory region—either projected shared memory (for IPC) or 
standard local process memory allocated via `malloc` or `new` (for inter-thread communication 
within a single process). Low-level allocation of memory blocks within these regions occurs 
through this class's methods.

**Key Methods:**

*   `auto Allocate(size_t size) -> Pointer<void>` — Allocates a block of a strictly defined size.
*   `auto AllocateAtLeast(size_t size) -> std::pair<Pointer<void>, size_t>` — Allocates a block 
    of the maximum possible size, but no less than `size`. This is used when the exact size of 
    a C++ object is unknown or when it may increase during reallocation. This method is 
    actively used by the higher-level manager, `sme::MemoryDomain`.
    *   **Crucial Note:** Once the memory within such a block is no longer being extended, its 
        size **must be fixed** using the `Resize` method. If this is not done, a significant 
        portion of memory (often extending to the end of the region) will remain unavailable, 
        forcing subsequent `Allocate` calls to look for other free blocks. It is recommended 
        to use `AllocateAtLeast` only in "one producer — multiple consumers" scenarios.
*   `auto Resize(Pointer<void> ptr, size_t new_size) -> bool` — Resizes a previously allocated 
    block.
*   `auto Deallocate(Pointer<void>& ptr) noexcept -> bool` — Frees an allocated block.

### Usage Scenarios

Two usage scenarios can be implemented for this class:

1.  **Primitive:** For allocating a small number of fixed-size objects with simple dynamic 
    data structures. Since `sme::MemorySpace` does not use a specialized memory block manager, 
    frequent allocation and deallocation may lead to high fragmentation and subsequent failures 
    when requesting medium or large memory blocks.
2.  **Standard:** Use the `sme::MemoryDomain` class.

### Class `sme::MemoryDomain`

Used as a memory manager for dynamic allocation. The primary purpose of `sme::MemoryDomain` is 
to replace `malloc`/`free` and `new`/`delete` within the shared memory context, specifically for 
allocating memory for C++ objects. It is also used by STL allocators for strings and containers.

The typical usage scenario for `sme::MemoryDomain` involves allocating memory and placing a 
single root object (DTO) within it, which then dynamically distributes memory for its internal 
structures from this specific `sme::MemoryDomain`. To allocate another root object, its own 
`sme::MemoryDomain` is created. This approach avoids the need for a global memory manager 
across the entire shared memory, which would require complex defragmentation and 
cross-process/cross-thread synchronization.

It is assumed that an individual `sme::MemoryDomain` will be used by a single writer thread to 
allocate memory for a specific object in one process, and subsequently by a reader thread 
using and deallocating that object in another process. Consequently, no synchronization is 
used for memory allocation/deallocation within `sme::MemoryDomain`.

Internally, `sme::MemoryDomain` manages multiple small memory blocks distributed within one or 
more larger blocks obtained via `sme::MemorySpace::AllocateAtLeast`/`Allocate`.

**Key Methods:**

*   `auto Allocate(size_t data_size) -> Pointer<void>` — Allocates memory. Upon the first call, 
    a block of at least `data_size` bytes is allocated via `sme::MemorySpace::AllocateAtLeast`. 
    Subsequent calls to `sme::MemoryDomain::Allocate` will utilize this block. If the block is 
    exhausted, another block of at least the requested size is allocated via 
    `sme::MemorySpace::AllocateAtLeast` and added to an internal list.
*   `void Deallocate(Pointer<void>& ptr) noexcept` — Frees memory.
*   `void DisableAllocationExtensible() noexcept` — Used when the writer finishes allocating 
    memory in its `sme::MemoryDomain`. This releases unused memory blocks from the free list via 
    `sme::MemorySpace::Deallocate` and shrinks the last utilized block to its minimum size via 
    `sme::MemorySpace::Resize`. This allows subsequent `sme::MemoryDomain` instances to efficiently 
    utilize the remaining shared memory. Further allocations in the current `sme::MemoryDomain` are 
    still possible using the remaining space in partially occupied blocks.

### Core Memory Space Functions

These are the fundamental functions used to initialize `sme::MemorySpace` and perform subsequent 
operations.

```cpp
auto sme::ConstructMemorySpace(
    MemoryMap& mem_map,
    size_t ofs = 0,
    Synchronizer::Type sync_type = Synchronizer::Type::kShared) -> MemorySpace*
```
Used after creating and mapping shared memory in one of the initialization processes to 
construct the `sme::MemorySpace` object.

```cpp
auto GetMemorySpace(MemoryMap& mem_map, size_t ofs = 0) -> MemorySpace&
```
Used after opening and mapping shared memory to gain access to an `sme::MemorySpace` object 
previously allocated (typically in another process) via `sme::ConstructMemorySpace`.

```cpp
template <typename T, typename... Arg>
auto sme::CreateRoot(MemorySpace& mem_space, Arg&&... args) -> T*
```
To start using objects in `sme::MemorySpace`, a primary root object of class `<T>` must be 
created. This object contains links to other objects located in the `sme::MemorySpace`. This 
method is called after `sme::ConstructMemorySpace`. For example, a class `<T>` object could 
contain a list of allocated `sme::MemoryDomain` instances.

```cpp
template <typename T>
auto sme::GetRoot(MemorySpace& mem_space) -> T&
```
Used to obtain a reference to the primary root object of class `<T>`. This method is called 
after `sme::GetMemorySpace`.

*   `auto sme::CreateMemoryDomain(sme::MemorySpace& mem_space, size_t domain_size, 
    sme::Synchronizer::Type sync_type) -> Pointer<sme::MemoryDomain>` — Creates an 
    `sme::MemoryDomain` within an `sme::MemorySpace`.
*   `void sme::DeleteMemoryDomain(sme::Pointer<sme::MemoryDomain>&) noexcept` — Deletes an 
    `sme::MemoryDomain` from the `sme::MemorySpace`.

For further details, see `demo/src/obj_writer.cpp` and `demo/src/obj_reader.cpp`.

### Core Memory Allocation Functions

As previously described, memory can be allocated from either `sme::MemorySpace` or 
`sme::MemoryDomain`. The following functions are provided for memory allocation and C++ 
object construction:

```cpp
template <typename T, typename MemoryAreaType, typename... Arg>
auto Create(MemoryAreaType& mem_area, Arg&&... args) -> T*
```
Allocates memory within a `MemoryAreaType` (either `sme::MemorySpace` or `sme::MemoryDomain`) 
and constructs an object of class `T` using the provided `args` for the constructor call.

```cpp
template <typename T, typename MemoryAreaType>
void Delete(MemoryAreaType& mem_area, sme::Pointer<T>& obj) noexcept
```
Invokes the destructor of class `T` for an object pointed to by `sme::Pointer<T>` within the 
memory area `MemoryAreaType` (either `sme::MemorySpace` or `sme::MemoryDomain`).

```cpp
template <typename T,
          typename MemoryAreaType,
          typename DeleterType = sme::Deleter<T, MemoryAreaType>,
          typename... Arg>
auto make_unique(MemoryAreaType& mem_area, Arg&&... args) -> std::unique_ptr<T, DeleterType>
```
Creates an object of class `T` in the memory area `MemoryAreaType` (`sme::MemorySpace` or 
`sme::MemoryDomain`).

```cpp
template <typename T, typename MemoryAreaType>
class BasicAllocator
```
A class for allocating elementary data types stored in STL strings and containers within the 
memory area `MemoryAreaType` (`sme::MemorySpace` or `sme::MemoryDomain`).

```cpp
template <typename MemoryAreaType>
class AllocationContext
```
Sets a thread-local global memory allocator within the `MemoryAreaType` (`sme::MemorySpace` or 
`sme::MemoryDomain`). Using this context allows you to avoid explicitly passing the allocator 
parameter to STL strings and containers.

### STL Adaptations and Allocators

The `sme/mdm` directory contains adaptations of STL classes and their respective allocators:

```cpp
template <typename T>
using MemoryDomainAllocator = BasicAllocator<T, sme::MemoryDomain>;

template <typename CharT, class Traits = std::char_traits<CharT>>
using basic_string = std::basic_string<CharT, Traits, MemoryDomainAllocator<CharT>>;

template <typename T>
using vector = std::vector<T, MemoryDomainAllocator<T>>;

template <typename T>
using list = std::list<T, MemoryDomainAllocator<T>>;
```
...and other similar containers.

### Message Queue

Often, data exchange requires a message queue that can contain various types of C++ class 
objects. The `sme::MessageChannel` class was developed for this purpose. For each message of 
class `sme::Message`, its own `sme::MemoryDomain` is formed, within which an object of an 
arbitrary C++ class is allocated using functions from the "Core Memory Allocation Functions" 
section.

The preferred usage pattern is "one producer — many consumers." This is because a message is 
created for an object with an unknown amount of allocated memory; as described above regarding 
`sme::MemoryDomain` specifics, it is better not to use multiple `sme::MemoryDomain` instances 
simultaneously. Once allocation for a specific object is complete, the message should be 
sent, and you can proceed to create the next one. If the expected memory size for the object is 
known in advance, several messages can be formed simultaneously (multiple `sme::MemoryDomain` 
instances), moving to a "many producers — many consumers" scheme.

**Class `sme::MessageChannel`:**

*   `MessageChannel::MessageChannel(MemoryMap mem_map, InitialState init_state)` — Creates a 
    message queue in shared memory.
*   `auto MessageChannel::GetWriter() -> sme::MessageWriter&` — Obtains the message writer 
    object.
*   `auto MessageChannel::GetReader() -> sme::MessageReader&` — Obtains the message reader 
    object.

**Class `sme::MessageWriter`:**

*   `auto MessageWriter::CreateMessage() -> sme::IntrusivePtr<sme::Message>` — Creates a 
    message to be filled with data. If the message does not need to be sent, simply release 
    ownership in the `IntrusivePtr<Message>`.
*   `auto MessageWriter::Commit(sme::IntrusivePtr<sme::Message>& msg) -> sme::QueueResult` — 
    Sends the message. Upon successful queuing, the `msg` object loses ownership of the message.

**Class `sme::MessageReader`:**

*   `auto sme::MessageReader::Read() -> std::pair<QueueResult, IntrusivePtr<Message>>` — 
    Reads a message without waiting (non-blocking).
*   `auto sme::MessageReader::Read(const std::chrono::milliseconds& timeout) -> 
    std::pair<QueueResult, IntrusivePtr<Message>>` — Reads a message, waiting for a timeout.
*   `auto sme::MessageReader::Wait(const std::chrono::milliseconds& timeout) const -> 
    QueueResult` — Waits for the queue to be ready for reading.

**Class `sme::Message`:**

*   **Data Filling Methods:**
    *   `auto sme::Message::GetAllocationDomain() -> sme::MemoryDomain&` — Calling this method 
        starts the data-filling phase. The object and its internal data are allocated within 
        the `sme::MemoryDomain`.
    *   `void SetRootObject(void* obj, size_t type_id)` — After allocating the object `obj`, 
        it must be set as the root so the reader can identify and access the primary message 
        object. The `type_id` is used to uniquely identify the C++ class of the root object.

*   **Data Reading Methods:**
    *   `auto sme::Message::GetRootObjectDefinition() const -> const 
        MessageRootObjectDefinition&` — Reads information about the root object and its type. 
        For example, knowing the type, you can use:
        `sme::Pointer<Entity> entity = msg->GetRootObjectDefinition().object;`

### Usage examples are provided in the `demo` directory:
*   **The basic methods ** are demonstrated in `demo/src/obj_writer.cpp` and `demo/src/obj_reader.cpp`.
*   **The message queue ** are demonstrated in `demo/src/channel_writer.cpp` and `demo/src/channel_reader.cpp`.

For implementation details, you can also refer to the sources in `include/sme` and `src` directory and the tests in the `tests` directory.

---
*Detailed documentation on usage is being written...*
