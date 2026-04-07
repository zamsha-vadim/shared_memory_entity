cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(sme LANGUAGES CXX)

include(GNUInstallDirs)
include(GenerateExportHeader)
include(CheckCXXCompilerFlag)

### Compile/Build settings

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64)")
    check_cxx_compiler_flag("-mcx16" COMPILER_HAS_MCX16)
    if(NOT COMPILER_HAS_MCX16)
        message(FATAL "Compiler doesn't support '-mcx16' option")
    endif()
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

#add_compile_options(-pipe -Wall -Wextra -Werror)
add_compile_options(-pipe -Wall -Wextra)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

option(SANITIZE_MEMORY "Sanitize memory" OFF)
if (SANITIZE_MEMORY)
    set(satitize_mem_flags "-fsanitize=address,pointer-compare,pointer-subtract,leak,undefined")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${satitize_mem_flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${satitize_mem_flags}")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-g0 -O3")

set(CMAKE_INCLUDE_CURRENT_DIR true)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

### Tools settings

enable_testing()
find_package(GTest)

option(CLANG_TIDY "Use Clang-tidy" OFF)
find_program(CLANG_TIDY_BIN 
    NAMES clang-tidy clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy-15 clang-tidy-14
)
if (CLANG_TIDY_BIN)
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_BIN}" "--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy" "--quiet")
    message (STATUS "clang-tidy found: ${CLANG_TIDY_BIN}")
else()
    message(STATUS "clang-tidy not found")
endif()    

### Project settings

set(BASE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(BUILD_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)
set(INCLUDE_DIR "${BASE_INCLUDE_DIR}/sme")

### mem_basis library

add_library(mem_basis 
    INTERFACE
        ${INCLUDE_DIR}/sme_export.h

        ${INCLUDE_DIR}/mem_ptr.h
        ${INCLUDE_DIR}/basic_allocator.h
)

target_include_directories(mem_basis 
    INTERFACE
        $<BUILD_INTERFACE:${BASE_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${BUILD_INCLUDE_DIR}>

        $<INSTALL_INTERFACE:include>
)

target_compile_options(mem_basis
    INTERFACE
        -fno-strict-aliasing
)

### mem_space_manip library

add_library(mem_space_manip OBJECT
    ${INCLUDE_DIR}/internal/mem_space_block.h
    src/mem_space_block.cpp

    ${INCLUDE_DIR}/internal/mem_space_manip.h
    src/mem_space_manip.cpp
)

generate_export_header(mem_space_manip
    BASE_NAME sme
    INCLUDE_GUARD_NAME SME_SME_EXPORT_H
    EXPORT_FILE_NAME ${INCLUDE_DIR}/sme_export.h
)

set_property(TARGET mem_space_manip
    PROPERTY
        POSITION_INDEPENDENT_CODE TRUE
)

target_link_libraries(mem_space_manip
    PUBLIC
        mem_basis
)

target_compile_options(mem_space_manip
    PRIVATE
        -fno-omit-frame-pointer
)

### mem_domain_manip library

add_library(mem_domain_manip OBJECT
    ${INCLUDE_DIR}/internal/mem_domain_block.h
    src/mem_domain_block.cpp

    ${INCLUDE_DIR}/internal/mem_domain_segment.h
    src/mem_domain_segment.cpp
)

target_compile_options(mem_domain_manip
    PRIVATE
        -fno-omit-frame-pointer
)

set_property(TARGET mem_domain_manip
    PROPERTY
        POSITION_INDEPENDENT_CODE TRUE
)

target_link_libraries(mem_domain_manip
    PUBLIC
        mem_basis
)

### sme library

add_library(sme SHARED
    ${INCLUDE_DIR}/internal/futex.h
    src/futex.cpp

    ${INCLUDE_DIR}/mutex.h
    src/mutex.cpp

    ${INCLUDE_DIR}/mem_space.h
    src/mem_space.cpp

    ${INCLUDE_DIR}/mem_domain.h
    src/mem_domain.cpp

    ${INCLUDE_DIR}/alloc_util.h
    ${INCLUDE_DIR}/internal/string_hash.h

    ${INCLUDE_DIR}/msp/unique_ptr.h
    ${INCLUDE_DIR}/msp/allocator.h
    ${INCLUDE_DIR}/msp/string.h
    ${INCLUDE_DIR}/msp/vector.h
    ${INCLUDE_DIR}/msp/deque.h
    ${INCLUDE_DIR}/msp/list.h
    ${INCLUDE_DIR}/msp/forward_list.h
    ${INCLUDE_DIR}/msp/map.h
    ${INCLUDE_DIR}/msp/unordered_map.h
    ${INCLUDE_DIR}/msp/set.h
    ${INCLUDE_DIR}/msp/unordered_set.h
    ${INCLUDE_DIR}/msp/queue.h
    ${INCLUDE_DIR}/msp/stack.h
    ${INCLUDE_DIR}/msp/root_obj.h

    ${INCLUDE_DIR}/mdm/unique_ptr.h
    ${INCLUDE_DIR}/mdm/allocator.h
    ${INCLUDE_DIR}/mdm/string.h
    ${INCLUDE_DIR}/mdm/vector.h
    ${INCLUDE_DIR}/mdm/deque.h
    ${INCLUDE_DIR}/mdm/list.h
    ${INCLUDE_DIR}/mdm/forward_list.h
    ${INCLUDE_DIR}/mdm/map.h
    ${INCLUDE_DIR}/mdm/unordered_map.h
    ${INCLUDE_DIR}/mdm/set.h
    ${INCLUDE_DIR}/mdm/unordered_set.h
    ${INCLUDE_DIR}/mdm/queue.h
    ${INCLUDE_DIR}/mdm/stack.h

    ${INCLUDE_DIR}/mem_map.h
    src/mem_map.cpp
    ${INCLUDE_DIR}/shm.h
    src/shm.cpp

    ${INCLUDE_DIR}/mapped_obj.h
    src/mapped_obj.cpp

    ${INCLUDE_DIR}/internal/ref_count.h

    ${INCLUDE_DIR}/internal/item_link.h
    ${INCLUDE_DIR}/internal/queue_res.h
    ${INCLUDE_DIR}/internal/lf_queue.h
    ${INCLUDE_DIR}/internal/queue.h

    ${INCLUDE_DIR}/msg_channel.h
    src/msg_channel.cpp
)

target_compile_options(sme
    PRIVATE
        -fno-omit-frame-pointer
)

target_link_libraries(sme
    mem_space_manip 
    mem_domain_manip 

    Threads::Threads
   -lrt
)

if (CMAKE_CXX_COMPILER_ID STREQUAL GNU OR CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    if (COMPILER_HAS_MCX16)
        target_compile_options(sme
            PUBLIC    
                -mcx16
        )
    endif()

    target_link_libraries(sme
       -latomic
    )   
endif()

#if (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
#    target_link_libraries(sme
#        -stdlib=libc++ 
#    )   
#endif()

if (GTest_FOUND)
    add_executable(sme_base_test
        test/mem_ptr_test.cpp
        test/mem_space_manip_test.cpp
        test/mem_domain_block_test.cpp
    )

    target_compile_options(sme_base_test
        PRIVATE
            -Wno-unused-value 
            -Wno-unused-but-set-variable
    )

    target_link_libraries(sme_base_test
        mem_space_manip 
        mem_domain_manip 

        GTest::gtest 
        GTest::gtest_main
    )

    add_test(NAME sme_base_test COMMAND sme_base_test)

    add_executable(sme_test
        test/mem_fault.h
        test/mem_fault.cpp

        test/mutex_test.cpp

        test/queue_test.cpp
        test/sync_queue_test.cpp
        test/lf_queue_test.cpp

        test/mem_space_test.cpp
        test/mem_space_alloc_test.cpp

        test/mem_domain_test.cpp
        test/mem_domain_alloc_test.cpp

        test/shm_test_util.h
        test/shm_test_util.cpp

        test/shm_test.cpp

        test/mdm_test.cpp

        test/msg_channel_test.cpp
    )

    target_compile_options(sme_test
        PRIVATE 
            -Wno-unused-value 
            -Wno-unused-but-set-variable
            -Wno-unused-function
            -Wno-unused-parameter
    )

    target_link_libraries(sme_test
       GTest::gtest 
       GTest::gtest_main
       sme
    )

    add_test(NAME sme_test COMMAND sme_test)
endif()

if (CLANG_TIDY AND CLANG_TIDY_BIN)
    set(SME_CLANG_TIDY 
        ${CLANG_TIDY_COMMAND}
        "--checks=-"
    )
    set_target_properties(sme 
        PROPERTIES 
            CXX_CLANG_TIDY "${SME_CLANG_TIDY}"
    )

#    if (GTest_FOUND)
#        set(TEST_CLANG_TIDY 
#            ${CLANG_TIDY_COMMAND}
#            "--checks=-clang-diagnostic-unused-variable,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers"
#        )
#        set_target_properties(sme_test
#            PROPERTIES 
#                CXX_CLANG_TIDY "${TEST_CLANG_TIDY}"
#        )
#    endif()
endif()

