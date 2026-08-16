include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

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
    test_sms_l3_messages.cpp
    test_gsm_specs.cpp
    test_threading.cpp
    test_parser.cpp
    test_visitor.cpp
    test_bcc_gcc_messages.cpp
    test_ls_messages.cpp
    test_extended_messages.cpp
    test_rr_builders.cpp
    test_mm_builders.cpp
    test_cc_builders.cpp
    test_lapdm.cpp
    test_dispatcher.cpp
    test_gmm_builders.cpp
    test_sm_builders.cpp
    test_full_roundtrip.cpp
    test_bts_pipeline.cpp
    test_ms_context.cpp
    test_l3_timer.cpp
    test_transaction.cpp
    test_state_machine.cpp
    test_channel_pool.cpp
    test_bts_procedures.cpp
    test_sharded_channel_pool.cpp
    test_zero_copy.cpp
    test_procedure_types.cpp
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
    target_compile_options(gsml3parser_tests PRIVATE /bigobj)
endif()

add_test(NAME gsml3parser_tests COMMAND gsml3parser_tests)
