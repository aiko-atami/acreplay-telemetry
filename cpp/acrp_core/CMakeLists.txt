add_library(acrp_core STATIC)
add_library(acrp::acrp_core ALIAS acrp_core)

target_sources(
  acrp_core
  PRIVATE src/api.cpp
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
      include/acrp/api.hpp
      include/acrp/channel_ids.hpp
      include/acrp/csv_export.hpp
      include/acrp/manifest.hpp
      include/acrp/pack_format.hpp
      include/acrp/parse_options.hpp
      include/acrp/replay_types.hpp
      include/acrp/status.hpp
)

target_include_directories(
  acrp_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(EMSCRIPTEN)
  target_compile_options(acrp_core PUBLIC -sUSE_ZLIB=1)
  target_link_options(acrp_core PUBLIC -sUSE_ZLIB=1)
else()
  target_link_libraries(acrp_core PUBLIC ZLIB::ZLIB)
endif()

acrp_enable_warnings(acrp_core)
acrp_enable_sanitizers(acrp_core)
