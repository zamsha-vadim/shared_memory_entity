cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(sme 
    VERSION 0.9
    DESCRIPTION "Shared memory entity"
    HOMEPAGE_URL "https://github.com/zamsha-vadim/shared_memory_entity"
    LANGUAGES CXX
)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|amd64)")
    message(FATAL_ERROR 
            "The project supports x86_64 and amd64 processors. You have '${CMAKE_SYSTEM_PROCESSOR}' processor")
endif()

include(CheckCXXCompilerFlag)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

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

### LABRARY: mem_basis

set(mem_basis_headers
    ${INCLUDE_DIR}/sme_export.h

    ${INCLUDE_DIR}/mem_ptr.h

    ${INCLUDE_DIR}/basic_allocator.h
    ${INCLUDE_DIR}/basic_allocator.tcc
)

add_library(mem_basis INTERFACE 
    ${mem_basis_headers}
)

target_include_directories(mem_basis 
    INTERFACE
        $<BUILD_INTERFACE:${BASE_INCLUDE_DIR}>
)

target_compile_options(mem_basis
    INTERFACE
        -fno-strict-aliasing
)

### LABRARY:mem_space_manip

set(mem_space_manip_headers
    ${INCLUDE_DIR}/internal/mem_space_block.h
    ${INCLUDE_DIR}/internal/mem_space_manip.h
)

add_library(mem_space_manip OBJECT
    ${mem_space_manip_headers}

    src/mem_space_block.cpp
    src/mem_space_manip.cpp
)

generate_export_header(mem_space_manip
    BASE_NAME sme
    INCLUDE_GUARD_NAME SME_SME_EXPORT_H
    EXPORT_FILE_NAME ${INCLUDE_DIR}/sme_export.h
)

set_target_properties(mem_space_manip
    PROPERTIES 
        POSITION_INDEPENDENT_CODE TRUE
)

target_compile_options(mem_space_manip
    PRIVATE
        -fno-omit-frame-pointer
)

target_link_libraries(mem_space_manip
    PUBLIC
        mem_basis
)

### LABRARY: mem_domain_manip

set(mem_domain_manip_headers
    ${INCLUDE_DIR}/internal/mem_domain_block.h
    ${INCLUDE_DIR}/internal/mem_domain_segment.h
)

add_library(mem_domain_manip OBJECT
    ${mem_domain_manip_headers}

    src/mem_domain_block.cpp
    src/mem_domain_segment.cpp
)

set_target_properties(mem_domain_manip
    PROPERTIES 
        POSITION_INDEPENDENT_CODE TRUE
)

target_compile_options(mem_domain_manip
    PRIVATE
        -fno-omit-frame-pointer
)

target_link_libraries(mem_domain_manip
    PUBLIC
        mem_basis
)

### LIBRARY: sme 

set(sme_public_headers
    ${INCLUDE_DIR}/ipvt.h

    ${INCLUDE_DIR}/futex.h
    ${INCLUDE_DIR}/mutex.h
    ${INCLUDE_DIR}/cond_var.h

    ${INCLUDE_DIR}/sync.h

    ${INCLUDE_DIR}/mem_space.h
    ${INCLUDE_DIR}/mem_domain.h

    ${INCLUDE_DIR}/alloc_util.h

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
    ${INCLUDE_DIR}/shm.h
    ${INCLUDE_DIR}/mapped_obj.h

    ${INCLUDE_DIR}/sync_type.h
    ${INCLUDE_DIR}/msg_channel.h
)

set(sme_private_headers
    ${INCLUDE_DIR}/internal/string_hash.h

    ${INCLUDE_DIR}/internal/ref_count.h
    ${INCLUDE_DIR}/internal/item_link.h
    ${INCLUDE_DIR}/internal/queue_res.h
    ${INCLUDE_DIR}/internal/lf_queue.h
    ${INCLUDE_DIR}/internal/lf_queue.tcc
    ${INCLUDE_DIR}/internal/queue.h
    ${INCLUDE_DIR}/internal/queue.tcc
)

set(sme_headers
    ${sme_public_headers}
    ${sme_private_headers}
)

add_library(sme SHARED
    ${sme_headers}

    src/futex.cpp
    src/mutex.cpp
    src/cond_var.cpp

    src/sync.cpp

    src/mem_space.cpp
    src/mem_domain.cpp
    src/mem_map.cpp

    src/shm.cpp
    src/mapped_obj.cpp

    src/item_link.cpp
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

# INSTALLATION

set(INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})

install(TARGETS mem_basis mem_space_manip mem_domain_manip sme 
    EXPORT SmeTargets
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    INCLUDES DESTINATION ${INSTALL_INCLUDE_DIR}
)

install(DIRECTORY ${INCLUDE_DIR}
    TYPE INCLUDE
    FILES_MATCHING PATTERN "*.h" PATTERN "*.tcc"
)

set(config_dir "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATAROOTDIR}/cmake")
set(config_file "${config_dir}/SmeConfig.cmake")
set(config_version_file "${config_dir}/SmeConfigVersion.cmake")
set(install_config_dir "${CMAKE_INSTALL_DATAROOTDIR}/cmake/Sme-${PROJECT_VERSION}")

write_basic_package_version_file(
    ${config_version_file}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/SmeConfig.cmake.in"
    "${config_file}"
    PATH_VARS 
        INSTALL_INCLUDE_DIR 
        INSTALL_LIB_DIR
    INSTALL_DESTINATION 
        ${install_config_dir}
)

install(EXPORT SmeTargets
    DESTINATION ${install_config_dir}
    NAMESPACE Sme::
)

install(
    FILES
        ${config_file}
        ${config_version_file}
    DESTINATION 
        ${install_config_dir}
)

export(EXPORT SmeTargets 
    FILE "${config_dir}/SmeTargets.cmake"
    NAMESPACE Sme::
)

# UNINSTALLATION

if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

# TESTS

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
        test/cond_var_test.cpp

        test/item_link_test.cpp

        test/queue_test.cpp
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
    set(SME_CLANG_TIDY ${CLANG_TIDY_COMMAND})
    set_target_properties(sme 
        PROPERTIES 
            CXX_CLANG_TIDY "${SME_CLANG_TIDY}"
    )

#    if (GTest_FOUND)
#        set(TEST_CLANG_TIDY 
#            ${CLANG_TIDY_COMMAND}
#            "--checks=-clang-diagnostic-unused-variable,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-readability-identifier-length,-cppcoreguidelines-avoid-non-const-global-variables,-concurrency-mt-unsafe,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-*"
#        )
#        set_target_properties(sme_test
#            PROPERTIES 
#                CXX_CLANG_TIDY "${TEST_CLANG_TIDY}"
#        )
#    endif()
endif()

