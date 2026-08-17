add_executable(example_parse_file example_parse_file.cpp)
target_link_libraries(example_parse_file PRIVATE gsml3parser)

add_executable(example_streaming example_streaming.cpp)
target_link_libraries(example_streaming PRIVATE gsml3parser)

add_executable(example_multithread example_multithread.cpp)
target_link_libraries(example_multithread PRIVATE gsml3parser)

add_executable(example_benchmark example_benchmark.cpp)
target_link_libraries(example_benchmark PRIVATE gsml3parser)

# ── BTS examples ───────────────────────────────────────────────────────

add_executable(example_bts_paging example_bts_paging.cpp)
target_link_libraries(example_bts_paging PRIVATE gsml3parser)

add_executable(example_bts_channel_assignment example_bts_channel_assignment.cpp)
target_link_libraries(example_bts_channel_assignment PRIVATE gsml3parser)

add_executable(example_bts_sysinfo example_bts_sysinfo.cpp)
target_link_libraries(example_bts_sysinfo PRIVATE gsml3parser)

add_executable(example_bts_dispatcher example_bts_dispatcher.cpp)
target_link_libraries(example_bts_dispatcher PRIVATE gsml3parser)

# ── Full BTS procedure examples ────────────────────────────────

add_executable(example_bts_rach_assignment example_bts_rach_assignment.cpp)
target_link_libraries(example_bts_rach_assignment PRIVATE gsml3parser)

add_executable(example_bts_paging_call example_bts_paging_call.cpp)
target_link_libraries(example_bts_paging_call PRIVATE gsml3parser)

add_executable(example_bts_location_update example_bts_location_update.cpp)
target_link_libraries(example_bts_location_update PRIVATE gsml3parser)

add_executable(example_benchmark_stack example_benchmark_stack.cpp)
target_link_libraries(example_benchmark_stack PRIVATE gsml3parser)

add_executable(example_zero_copy example_zero_copy.cpp)
target_link_libraries(example_zero_copy PRIVATE gsml3parser)

add_executable(example_lapdm_entity example_lapdm_entity.cpp)
target_link_libraries(example_lapdm_entity PRIVATE gsml3parser)

add_executable(example_subscriber_registry example_subscriber_registry.cpp)
target_link_libraries(example_subscriber_registry PRIVATE gsml3parser)

# ── Procedure Framework examples ────────────────────────────────

add_executable(example_procedure_location_update example_procedure_location_update.cpp)
target_link_libraries(example_procedure_location_update PRIVATE gsml3parser)

add_executable(example_procedure_call_setup example_procedure_call_setup.cpp)
target_link_libraries(example_procedure_call_setup PRIVATE gsml3parser)

add_executable(example_rsl_pipeline example_rsl_pipeline.cpp)
target_link_libraries(example_rsl_pipeline PRIVATE gsml3parser)

add_executable(example_reference_bts example_reference_bts.cpp)
target_link_libraries(example_reference_bts PRIVATE gsml3parser)

if(MSVC)
    foreach(_tgt example_parse_file example_streaming example_multithread example_benchmark
                   example_bts_paging example_bts_channel_assignment example_bts_sysinfo
                   example_bts_dispatcher example_bts_rach_assignment example_bts_paging_call
                   example_bts_location_update example_benchmark_stack example_zero_copy
                   example_lapdm_entity example_subscriber_registry
                   example_procedure_location_update example_procedure_call_setup
                   example_rsl_pipeline example_reference_bts)
        target_compile_definitions(${_tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${_tgt} PRIVATE /bigobj)
    endforeach()
endif()
