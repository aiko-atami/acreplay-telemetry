function(acrp_enable_sanitizers target_name)
  if(MSVC OR EMSCRIPTEN)
    return()
  endif()

  set(acrp_sanitizer_flags "")

  if(ACRP_ENABLE_ASAN)
    list(APPEND acrp_sanitizer_flags -fsanitize=address)
  endif()

  if(ACRP_ENABLE_UBSAN)
    list(APPEND acrp_sanitizer_flags -fsanitize=undefined)
  endif()

  if(acrp_sanitizer_flags)
    target_compile_options(${target_name} PRIVATE "$<$<CONFIG:Debug>:${acrp_sanitizer_flags}>")
    target_link_options(${target_name} PRIVATE "$<$<CONFIG:Debug>:${acrp_sanitizer_flags}>")
  endif()
endfunction()
