cmake_minimum_required(VERSION 3.10)

project(HashTableProject VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

include_directories(${PROJECT_SOURCE_DIR}/src)

file(GLOB SOURCES
    src/*.c
)

add_executable(HashTable ${SOURCES})

if(UNIX)
    target_link_libraries(HashTable m)
endif()

# optional
target_compile_options(HashTable PRIVATE -Wall -Wextra -pedantic)
