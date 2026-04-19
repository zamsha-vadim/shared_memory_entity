cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(sme_demo 
    LANGUAGES CXX
)

if(NOT Sme_DIR)
    if(EXISTS ../build)
        set(Sme_DIR ../build/share/cmake)
    endif()
endif()

### Compile/Build settings

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

find_package(Sme REQUIRED)

add_compile_options(-pipe -Wall -Wextra)

add_executable(sme_writer
    src/sme_writer.cpp
)

target_link_libraries(sme_writer
    Threads::Threads
    Sme::sme
)

add_executable(sme_reader
    src/sme_reader.cpp
)

target_link_libraries(sme_reader
    Threads::Threads
    Sme::sme
)
