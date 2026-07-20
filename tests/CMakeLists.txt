include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(
    unit_tests
    test_token_bucket.cpp
    test_sharded_rate_limiter.cpp
    test_leader_elector.cpp
    test_redis_backend.cpp
)

target_link_libraries(unit_tests
    PRIVATE ratelimiter GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)