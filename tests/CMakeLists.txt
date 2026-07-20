include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

# ---------- Unit Tests ----------
file(GLOB UNIT_TESTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp"
)

add_executable(unit_tests ${UNIT_TESTS})

target_link_libraries(unit_tests
    PRIVATE
        ratelimiter
        GTest::gtest_main
)

gtest_discover_tests(unit_tests)

# ---------- Smoke Tests ----------
file(GLOB SMOKE_TESTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*smoke_test.cpp"
)

foreach(TEST_FILE ${SMOKE_TESTS})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE ratelimiter)
endforeach()

# ---------- Manual Tests ----------
file(GLOB MANUAL_TESTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/manual_*.cpp"
)

foreach(TEST_FILE ${MANUAL_TESTS})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE ratelimiter)
endforeach()