if(NOT EMSCRIPTEN)
  add_executable(acrp_cli main.cpp)
  target_link_libraries(acrp_cli PRIVATE acrp_core)

  include(CompilerWarnings)
  include(Sanitizers)

  acrp_enable_warnings(acrp_cli)
  acrp_enable_sanitizers(acrp_cli)
endif()
