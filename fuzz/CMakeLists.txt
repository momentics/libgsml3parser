# Fuzzing targets (audit P2-8: the ENABLE_FUZZING option was declared
# but never used). libFuzzer requires Clang/LLVM; on MSVC the option is
# a no-op with a status message.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Register the smoke tests even when BUILD_TESTS is OFF: the CI
    # fuzz job configures with -DBUILD_TESTS=OFF and runs ctest on the
    # *_smoke tests.
    enable_testing()
    set(GSML3_FUZZ_TARGETS
        fuzz_parse_l3
        fuzz_rsl_parse
        fuzz_lapdm_decode
        fuzz_framer)
    foreach(t IN LISTS GSML3_FUZZ_TARGETS)
        add_executable(${t} ${t}.cpp)
        target_link_libraries(${t} PRIVATE gsml3parser)
        target_compile_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined)
        target_link_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined)
        # Smoke run so CI without a corpus still exercises the targets.
        add_test(NAME ${t}_smoke COMMAND ${t} -runs=2000)
    endforeach()
else()
    message(STATUS "ENABLE_FUZZING: libFuzzer requires Clang/LLVM; skipping fuzz targets (compiler: ${CMAKE_CXX_COMPILER_ID})")
endif()
