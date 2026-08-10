include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# All Phase 1-5 tests
add_executable(gsml3parser_tests
    test_expected.cpp
    test_bitreader.cpp
    test_parser_config.cpp
    test_l3header.cpp
    test_byte_source.cpp
    test_framer.cpp
    test_stream_processor.cpp
    test_golden_rr.cpp
    test_golden_mm.cpp
    test_golden_cc.cpp
    test_golden_ie.cpp
    test_roundtrip.cpp
    test_messages.cpp
    test_mm_messages.cpp
    test_cc_messages.cpp
    test_ss_messages.cpp
    test_gmm_messages.cpp
    test_sm_messages.cpp
    test_sms_messages.cpp
    test_gsm_specs.cpp
    test_threading.cpp
    test_parser.cpp
    test_visitor.cpp
)

find_package(Threads REQUIRED)

target_link_libraries(gsml3parser_tests
    PRIVATE
        gsml3parser
        GTest::gtest_main
        Threads::Threads
)

if(MSVC)
    target_compile_definitions(gsml3parser_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_test(NAME gsml3parser_tests COMMAND gsml3parser_tests)
