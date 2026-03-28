add_library(acrp_core STATIC)

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

target_include_directories(acrp_core PUBLIC include)

if(EMSCRIPTEN)
  target_compile_options(acrp_core PUBLIC -sUSE_ZLIB=1)
  target_link_options(acrp_core PUBLIC -sUSE_ZLIB=1)
else()
  find_package(ZLIB REQUIRED)
  target_link_libraries(acrp_core PUBLIC ZLIB::ZLIB)
endif()

include(CompilerWarnings)
include(Sanitizers)

acrp_enable_warnings(acrp_core)
acrp_enable_sanitizers(acrp_core)
