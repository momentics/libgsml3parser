include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(gsml3parser_tests
    test_bitvector.cpp
    test_parser.cpp
    test_messages.cpp
)

target_link_libraries(gsml3parser_tests
    PRIVATE
        gsml3parser
        GTest::gtest_main
)

add_test(NAME gsml3parser_tests COMMAND gsml3parser_tests)
