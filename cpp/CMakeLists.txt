cmake_minimum_required(VERSION 3.28...4.3)

project(acreplay-parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(ACRP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ACRP_ENABLE_ASAN "Enable AddressSanitizer in Debug builds" OFF)
option(ACRP_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer in Debug builds" OFF)
option(ACRP_ENABLE_CLANG_TIDY "Run clang-tidy during CMake builds when available" OFF)

if(ACRP_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(ACRP_CLANG_TIDY_ARGS "${CLANG_TIDY_EXE}" "--extra-arg=-Wno-unknown-warning-option")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            list(APPEND ACRP_CLANG_TIDY_ARGS "--extra-arg=--gcc-toolchain=/usr")
        endif()
        set(CMAKE_CXX_CLANG_TIDY "${ACRP_CLANG_TIDY_ARGS}")
    endif()
endif()

add_subdirectory(acrp_core)
add_subdirectory(acrp_cli)
add_subdirectory(acrp_wasm)

enable_testing()
add_subdirectory(tests)
