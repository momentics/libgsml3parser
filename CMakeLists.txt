cmake_minimum_required(VERSION 3.20)
project(gsml3parser
    VERSION 0.4.0
    DESCRIPTION "GSM L3 Signalling Message Parser — standalone C++20 library"
    LANGUAGES CXX
    HOMEPAGE_URL "https://github.com/momentics/libgsml3parser"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(ENABLE_FUZZING "Build fuzzing target" OFF)

# ── Source files ──────────────────────────────────────────────────────

set(CORE_SOURCES
    src/bitvector.cpp
    src/l3frame.cpp
    src/l3message.cpp
    src/common/l3common.cpp
    src/enums.cpp
    src/gsm_common.cpp
    src/logger.cpp
    src/parser.cpp
    src/mm/l3mmlements.cpp
    src/cc/l3cclements.cpp
)

set(RR_SOURCES
    src/rr/l3rrmessages.cpp
)

set(MM_SOURCES
    src/mm/l3mmmessages.cpp
)

set(CC_SOURCES
    src/cc/l3ccmessages.cpp
)

set(SS_SOURCES
    src/ss/l3ssmessages.cpp
)

# ── Public headers ────────────────────────────────────────────────────

set(PUBLIC_HEADERS
    include/gsml3parser/bitvector.h
    include/gsml3parser/l3frame.h
    include/gsml3parser/l3message.h
    include/gsml3parser/types.h
    include/gsml3parser/enums.h
    include/gsml3parser/scalar_types.h
    include/gsml3parser/gsm_common.h
    include/gsml3parser/logger.h
    include/gsml3parser/parser.h
    include/gsml3parser/common/l3common.h
    include/gsml3parser/rr/l3rrmessages.h
    include/gsml3parser/mm/l3mmmessages.h
    include/gsml3parser/cc/l3ccmessages.h
    include/gsml3parser/ss/l3ssmessages.h
)

# ── Library target ────────────────────────────────────────────────────

add_library(gsml3parser
    ${CORE_SOURCES}
    ${RR_SOURCES}
    ${MM_SOURCES}
    ${CC_SOURCES}
    ${SS_SOURCES}
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
