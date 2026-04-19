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

### obj_writer

add_executable(obj_writer
    src/obj_writer.cpp
)

target_link_libraries(obj_writer
    Threads::Threads
    Sme::sme
)

### obj_reader

add_executable(obj_reader
    src/obj_reader.cpp
)

target_link_libraries(obj_reader
    Threads::Threads
    Sme::sme
)

### entity

add_library(entity OBJECT
    src/entity.h
    src/entity.cpp
)

target_link_libraries(entity
    PUBLIC
        Sme::sme
)

### channel_writer

add_executable(channel_writer
    src/channel_writer.cpp
)

target_link_libraries(channel_writer
    Threads::Threads
    entity
)

### channel_reader

add_executable(channel_reader
    src/channel_reader.cpp
)

target_link_libraries(channel_reader
    Threads::Threads
    entity
)

