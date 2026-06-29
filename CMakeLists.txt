cmake_minimum_required(VERSION 3.16)
project(distributed_rate_limiter CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Threads REQUIRED)

find_library(REDIS_PLUS_PLUS_LIB redis++)
find_library(HIREDIS_LIB hiredis)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(ratelimiter
  src/token_bucket.cpp
  src/thread_safe_token_bucket.cpp
  src/sharded_rate_limiter.cpp
  src/redis_backend.cpp
  src/leader_elector.cpp
)

target_link_libraries(ratelimiter PUBLIC Threads::Threads)

if(REDIS_PLUS_PLUS_LIB AND HIREDIS_LIB)
  target_link_libraries(ratelimiter PUBLIC ${REDIS_PLUS_PLUS_LIB} ${HIREDIS_LIB})
  if(WIN32)
    target_link_libraries(ratelimiter PUBLIC ws2_32)
  endif()
else()
  message(WARNING "redis++/hiredis not found")
endif()

add_executable(server src/server_main.cpp)
target_link_libraries(server PRIVATE ratelimiter)

add_executable(bench src/bench_main.cpp)
target_link_libraries(bench PRIVATE ratelimiter)

add_executable(bench_contention src/bench_contention_main.cpp)
target_link_libraries(bench_contention PRIVATE Threads::Threads)