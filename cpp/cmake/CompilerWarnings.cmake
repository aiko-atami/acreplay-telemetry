function(acrp_enable_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
    if(ACRP_WARNINGS_AS_ERRORS)
      target_compile_options(${target_name} PRIVATE /WX)
    endif()
  else()
    set(acrp_warning_flags
        -Wall
        -Wextra
        -Wshadow
        -Wpedantic
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmisleading-indentation
        -Wnull-dereference
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      list(APPEND acrp_warning_flags -Wduplicated-cond)
    endif()

    if(ACRP_WARNINGS_AS_ERRORS)
      list(APPEND acrp_warning_flags -Werror)
    endif()

    target_compile_options(${target_name} PRIVATE ${acrp_warning_flags})
  endif()
endfunction()
