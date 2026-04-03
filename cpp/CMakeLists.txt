cmake_minimum_required(VERSION 3.28...4.3)

project(acreplay-parser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Sanitizers)

option(ACRP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ACRP_ENABLE_ASAN "Enable AddressSanitizer in Debug builds" OFF)
option(ACRP_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer in Debug builds" OFF)

if(NOT EMSCRIPTEN)
  find_package(ZLIB REQUIRED)
endif()

add_subdirectory(acrp_core)
add_subdirectory(acrp_cli)
add_subdirectory(acrp_wasm)

enable_testing()
add_subdirectory(tests)
