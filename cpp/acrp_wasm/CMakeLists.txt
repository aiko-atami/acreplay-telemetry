if(EMSCRIPTEN)
  add_executable(acrp_wasm exports.cpp)
  target_link_libraries(acrp_wasm PRIVATE acrp_core)

  include(EmscriptenSettings)
  acrp_apply_emscripten_settings(acrp_wasm)
else()
  add_custom_target(
    acrp_wasm
    COMMAND ${CMAKE_COMMAND} -E echo "acrp_wasm is only available with an Emscripten toolchain"
  )
endif()
