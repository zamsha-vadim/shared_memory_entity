cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project(sme_perf_test LANGUAGES CXX)

if(NOT Sme_DIR)
    if(EXISTS ../build)
        set(Sme_DIR ../build/share/cmake)
    endif()
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

find_package(Sme REQUIRED)

add_compile_options(-pipe -Wall -Wextra -O3)

add_executable(string_benchmark string_benchmark.cpp)
target_link_libraries(string_benchmark PRIVATE Threads::Threads Sme::sme)

add_executable(vector_benchmark vector_benchmark.cpp)
target_link_libraries(vector_benchmark PRIVATE Threads::Threads Sme::sme)

add_executable(deque_benchmark deque_benchmark.cpp)
target_link_libraries(deque_benchmark PRIVATE Threads::Threads Sme::sme)
