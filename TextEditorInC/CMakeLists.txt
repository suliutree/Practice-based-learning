cmake_minimum_required(VERSION 3.10)

project(TextEditorProject VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

include_directories(${PROJECT_SOURCE_DIR}/src)

file(GLOB SOURCES
    src/*.c
)

add_executable(TextEditor ${SOURCES})
