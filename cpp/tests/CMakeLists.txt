if(NOT EMSCRIPTEN)
  add_executable(acrp_tests_smoke smoke.cpp)
  target_link_libraries(acrp_tests_smoke PRIVATE acrp_core)
  add_test(NAME acrp_tests_smoke COMMAND acrp_tests_smoke)

  add_executable(acrp_tests_wasm_abi wasm_abi.cpp ../acrp_wasm/exports.cpp)
  target_link_libraries(acrp_tests_wasm_abi PRIVATE acrp_core)
  target_compile_definitions(acrp_tests_wasm_abi PRIVATE ACRP_WASM_NO_MAIN)
  add_test(NAME acrp_tests_wasm_abi COMMAND acrp_tests_wasm_abi)

  add_executable(acrp_export_pack export_pack.cpp)
  target_link_libraries(acrp_export_pack PRIVATE acrp_core)

  foreach(t acrp_tests_smoke acrp_tests_wasm_abi acrp_export_pack)
    acrp_enable_warnings(${t})
    acrp_enable_sanitizers(${t})
  endforeach()
endif()
