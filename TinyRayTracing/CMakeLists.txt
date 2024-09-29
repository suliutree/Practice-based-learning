cmake_minimum_required(VERSION 2.8)
project(TinyRayTracing)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB SOURCES *.h *.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})