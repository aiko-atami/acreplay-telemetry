function(acrp_apply_emscripten_settings target_name)
  if (NOT EMSCRIPTEN)
    return()
  endif()

  target_link_options(
    ${target_name}
    PRIVATE
      -sMODULARIZE=1
      -sEXPORT_ES6=1
      -sENVIRONMENT=worker,node
      -sALLOW_MEMORY_GROWTH=1
      -sFILESYSTEM=0
      -sNO_EXIT_RUNTIME=1
      -sEXPORTED_RUNTIME_METHODS=["HEAPU8","getValue","UTF8ToString"]
  )
endfunction()
