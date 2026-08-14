cmake_minimum_required(VERSION 3.20)

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

project(gsml3parser
    VERSION 0.11.0
    DESCRIPTION "GSM L3 Signalling Message Parser - standalone C++20 library"
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/momentics/libgsml3parser"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(ENABLE_FUZZING "Build fuzzing target" OFF)

# ── Source files ──────────────────────────────────────────────────────

set(CORE_SOURCES
    src/arena.cpp
    src/bitreader.cpp
    src/bitwriter.cpp
    src/gsm_common.cpp
    src/l3header.cpp
    src/common/l3common.cpp
    src/enums.cpp
    src/parser.cpp
    src/visitor.cpp
    src/bitstream/byte_source.cpp
    src/bitstream/framer.cpp
    src/bitstream/stream_processor.cpp
    src/lapdm.cpp
    src/dispatcher.cpp
    src/stack/ms_context.cpp
    src/stack/l3_timer.cpp
    src/stack/state_machine.cpp
    src/stack/transaction.cpp
    src/stack/channel_pool.cpp
)

set(RR_SOURCES
    src/rr/l3rrmessages.cpp
)

set(MM_SOURCES
    src/mm/l3mmelements.cpp
    src/mm/l3mmmessages.cpp
)

set(CC_SOURCES
    src/cc/l3ccelements.cpp
    src/cc/l3ccmessages.cpp
)

set(SS_SOURCES
    src/ss/l3ssmessages.cpp
)

set(GMM_SOURCES
    src/gmm/l3gmmmessages.cpp
    src/gmm/l3gmmelements.cpp
)

set(SM_SOURCES
    src/sm/l3smmessages.cpp
    src/sm/l3smelements.cpp
)

set(SMS_SOURCES
    src/sms/l3smsmessages.cpp
    src/sms/l3smselements.cpp
    src/sms/l3smsl3messages.cpp
)

set(BCC_SOURCES
    src/bcc/l3bccmessages.cpp
)

set(GCC_SOURCES
    src/gcc/l3gccmessages.cpp
)

set(LS_SOURCES
    src/ls/l3lsmessages.cpp
)

set(EXTENDED_SOURCES
    src/extended/l3extendedmessages.cpp
)

set(TESTPROC_SOURCES
    src/testproc/l3testproceduremessages.cpp
)

# ── Public headers ────────────────────────────────────────────────────

set(PUBLIC_HEADERS
    include/gsml3parser/arena.h
    include/gsml3parser/expected.h
    include/gsml3parser/bitreader.h
    include/gsml3parser/bitwriter.h
    include/gsml3parser/types.h
    include/gsml3parser/enums.h
    include/gsml3parser/protocol_types.h
    include/gsml3parser/gsm_common.h
    include/gsml3parser/parser_config.h
    include/gsml3parser/l3header.h
    include/gsml3parser/parser.h
    include/gsml3parser/common/l3common.h
    include/gsml3parser/rr/l3rrmessages.h
    include/gsml3parser/mm/l3mmelements.h
    include/gsml3parser/mm/l3mmmessages.h
    include/gsml3parser/cc/l3ccmessages.h
    include/gsml3parser/cc/l3ccelements.h
    include/gsml3parser/ss/l3ssmessages.h
    include/gsml3parser/gmm/l3gmmelements.h
    include/gsml3parser/gmm/l3gmmmessages.h
    include/gsml3parser/sm/l3smelements.h
    include/gsml3parser/sm/l3smmessages.h
    include/gsml3parser/sms/l3smselements.h
    include/gsml3parser/sms/l3smsmessages.h
    include/gsml3parser/sms/l3smsl3messages.h
    include/gsml3parser/bcc/l3bccmessages.h
    include/gsml3parser/gcc/l3gccmessages.h
    include/gsml3parser/ls/l3lsmessages.h
    include/gsml3parser/extended/l3extendedmessages.h
    include/gsml3parser/testproc/l3testproceduremessages.h
    include/gsml3parser/message_types.h
    include/gsml3parser/visitor.h
    include/gsml3parser/gsml3parser.hpp
    include/gsml3parser/bitstream/byte_source.h
    include/gsml3parser/bitstream/framer.h
    include/gsml3parser/bitstream/stream_processor.h
    include/gsml3parser/lapdm.h
    include/gsml3parser/dispatcher.h
    include/gsml3parser/stack/ms_context.h
    include/gsml3parser/stack/l3_timer.h
    include/gsml3parser/stack/state_machine.h
    include/gsml3parser/stack/transaction.h
    include/gsml3parser/stack/channel_pool.h
)

# ── Library target ────────────────────────────────────────────────────

add_library(gsml3parser
    ${CORE_SOURCES}
    ${RR_SOURCES}
    ${MM_SOURCES}
    ${CC_SOURCES}
    ${SS_SOURCES}
    ${GMM_SOURCES}
    ${SM_SOURCES}
    ${SMS_SOURCES}
    ${BCC_SOURCES}
    ${GCC_SOURCES}
    ${LS_SOURCES}
    ${EXTENDED_SOURCES}
    ${TESTPROC_SOURCES}
)

add_library(gsml3parser::parser ALIAS gsml3parser)

target_include_directories(gsml3parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(gsml3parser PUBLIC cxx_std_20)

target_compile_definitions(gsml3parser PRIVATE
    GSML3PARSER_EXPORTS
)

if(MSVC)
    target_compile_definitions(gsml3parser PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gsml3parser PRIVATE /W4 /WX- /bigobj)
    target_compile_options(gsml3parser PRIVATE $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(gsml3parser PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gsml3parser PRIVATE $<$<CONFIG:Release>:-O2>)
endif()

# ── Install ───────────────────────────────────────────────────────────

include(GNUInstallDirs)

install(TARGETS gsml3parser
    EXPORT gsml3parserTargets
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/gsml3parser
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT gsml3parserTargets
    FILE     gsml3parserTargets.cmake
    NAMESPACE gsml3parser::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gsml3parser
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/gsml3parserConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gsml3parserConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/gsml3parserConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gsml3parser
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/gsml3parserConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/gsml3parserConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gsml3parser
)

install(FILES
    COPYING
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/gsml3parser
)

# ── Tests ─────────────────────────────────────────────────────────────

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Examples ──────────────────────────────────────────────────────────

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ── Benchmarks ────────────────────────────────────────────────────────

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
