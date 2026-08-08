include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Phase 1-2 tests only (Phase 5 migrates remaining tests)
add_executable(gsml3parser_tests
    test_expected.cpp
    test_bitreader.cpp
    test_parser_config.cpp
    test_l3header.cpp
)

find_package(Threads REQUIRED)

target_link_libraries(gsml3parser_tests
    PRIVATE
        gsml3parser
        GTest::gtest_main
        Threads::Threads
)

add_test(NAME gsml3parser_tests COMMAND gsml3parser_tests)
