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

if(MSVC)
    foreach(_tgt example_parse_file example_streaming example_multithread example_benchmark
                 example_bts_paging example_bts_channel_assignment example_bts_sysinfo
                 example_bts_dispatcher)
        target_compile_definitions(${_tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endforeach()
endif()
