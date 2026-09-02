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
    test_arena.cpp
    test_flat_map.cpp
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
    test_flat_handler.cpp
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
    test_procedure_location_update.cpp
    test_response_builder.cpp
    test_subscriber_registry.cpp
    test_procedure_authentication.cpp
    test_procedure_call_setup_mo.cpp
    test_procedure_call_setup_mt.cpp
    test_procedure_channel_assignment.cpp
    test_procedure_ciphering_mode.cpp
    test_procedure_paging.cpp
    test_procedure_handover.cpp
    test_procedure_runner.cpp
    test_response_sink.cpp
    test_procedure_integration.cpp
    test_rsl_types.cpp
    test_rsl_parser.cpp
    test_rsl_builder.cpp
    test_high_load_stress.cpp
    test_procedure_call_release.cpp
    test_procedure_imsi_detach.cpp
    test_procedure_orchestrator.cpp
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

# GSML3PARSER_DEBUG: wall-clock performance budgets only hold for optimized
# builds; skip them in Debug (unoptimized) builds as well (Release/Debug
# codegen differs by an order of magnitude).
target_compile_definitions(gsml3parser_tests PRIVATE $<$<CONFIG:Debug>:GSML3PARSER_DEBUG>)

if(ENABLE_ASAN AND MSVC)
    # GSML3PARSER_ASAN: lets tests skip wall-clock performance budgets, which
    # are not meaningful under a sanitizer (2-3x slowdown).
    target_compile_definitions(gsml3parser_tests PRIVATE GSML3PARSER_ASAN)
    # /Zi is required: MSVC emits C5072 (ASan without debug info) in Release,
    # which gtest's /WX escalates to an error; /Zi also improves ASan reports.
    # The ASan runtime is linked automatically: link.exe detects the ASAN
    # metadata in the instrumented objects (LNK4300) and links the runtime.
    # /fsanitize=address is a compiler option only — passing it to link.exe
    # is invalid (LNK4044), so no link options are added here.
    target_compile_options(gsml3parser_tests PRIVATE /fsanitize=address /Zi)
    # gtest must be compiled with ASAN to avoid LNK2038 mismatches
    target_compile_options(gtest PRIVATE /fsanitize=address /Zi)
    target_compile_options(gtest_main PRIVATE /fsanitize=address /Zi)
    # /DEBUG keeps the debug info in the linked test exe (silences LNK4302
    # and yields detailed ASan reports).
    target_link_options(gsml3parser_tests PRIVATE /DEBUG)

    # Copy ASAN runtime DLL to test output directory
    get_filename_component(_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_asan_dbg_dll "${_msvc_bin_dir}/clang_rt.asan_dbg_dynamic-x86_64.dll")
    set(_asan_rel_dll "${_msvc_bin_dir}/clang_rt.asan_dynamic-x86_64.dll")

    # Note: POST_BUILD commands run for every configuration; both runtime DLLs
    # are copied (copy_if_different is idempotent) so the test exe always finds
    # the one matching its build configuration.
    if(EXISTS "${_asan_dbg_dll}")
        add_custom_command(TARGET gsml3parser_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_asan_dbg_dll}"
                $<TARGET_FILE_DIR:gsml3parser_tests>
            COMMENT "Copying ASAN runtime DLL (Debug)"
        )
    endif()
    if(EXISTS "${_asan_rel_dll}")
        add_custom_command(TARGET gsml3parser_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_asan_rel_dll}"
                $<TARGET_FILE_DIR:gsml3parser_tests>
            COMMENT "Copying ASAN runtime DLL (Release)"
        )
    endif()
endif()

add_test(NAME gsml3parser_tests COMMAND gsml3parser_tests)
