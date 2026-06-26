cmake_minimum_required(VERSION 3.16)
project(distributed_rate_limiter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(USE_TSAN "Build with ThreadSanitizer" OFF)
if(USE_TSAN)
    add_compile_options(-fsanitize=thread -g -01)
    add_link_options(-fsanitize=thread)
endif()

find_package(Threads REQUIRED)

find_library(REDIS_PLUS_PLUS_LIB redis++)
find_library(HIREDIS_LIB hiredis)

include_directories(${PROJECT_SOURCE_DIR}/include)