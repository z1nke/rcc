include (CheckCXXSourceCompiles)
include (CheckCXXCompilerFlag)

if (RCC_ENABLE_ASSERTIONS)
  message(STATUS "Assertions enabled")
  # MSVC doesn't like _DEBUG on release builds. See PR 4379.
  if (NOT MSVC)
    add_definitions(-D_DEBUG)
  endif ()
  # On non-Debug builds cmake automatically defines NDEBUG, so we
  # explicitly undefine it:
  if (NOT uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
    # NOTE: use `add_compile_options` rather than `add_definitions` since
    # `add_definitions` does not support generator expressions.
    add_compile_options($<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-UNDEBUG>)
    if (MSVC)
      # Also remove /D NDEBUG to avoid MSVC warnings about conflicting defines.
      foreach (flags_var_to_scrub
          CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO
          CMAKE_CXX_FLAGS_MINSIZEREL
          CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_RELWITHDEBINFO
          CMAKE_C_FLAGS_MINSIZEREL)
        string (REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " "
          "${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
      endforeach ()
     endif ()
  endif ()
endif ()

set(CXX_WARNING_FLAGS "")
set(C_WARNING_FLAGS "")

# -Wunused-parameter fires a lot so for now suppress it.
set(GCC_AND_CLANG_WARNINGS_CXX
  "-Wall"
)
set(GCC_AND_CLANG_WARNINGS_C
  "-Wall"
)
set(GCC_ONLY_WARNINGS_C "")
set(GCC_ONLY_WARNINGS_CXX "")
set(CLANG_ONLY_WARNINGS_C "")
set(CLANG_ONLY_WARNINGS_CXX "")

set(MSVC_ONLY_WARNINGS_C "/W4")
set(MSVC_ONLY_WARNINGS_CXX "/W4")

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  list(APPEND CXX_WARNING_FLAGS_TO_CHECK ${GCC_AND_CLANG_WARNINGS_CXX})
  list(APPEND CXX_WARNING_FLAGS_TO_CHECK ${GCC_ONLY_WARNINGS_CXX})
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  list(APPEND CXX_WARNING_FLAGS_TO_CHECK ${GCC_AND_CLANG_WARNINGS_CXX})
  list(APPEND CXX_WARNING_FLAGS_TO_CHECK ${CLANG_ONLY_WARNINGS_CXX})
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
  list(APPEND CXX_WARNING_FLAGS_TO_CHECK ${MSVC_ONLY_WARNINGS_C})
else ()
  message(WARNING "Unknown compiler")
endif()

if ("${CMAKE_C_COMPILER_ID}" MATCHES "GNU")
  list(APPEND C_WARNING_FLAGS_TO_CHECK ${GCC_AND_CLANG_WARNINGS_C})
  list(APPEND C_WARNING_FLAGS_TO_CHECK ${GCC_ONLY_WARNINGS_C})
elseif ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
  list(APPEND C_WARNING_FLAGS_TO_CHECK ${GCC_AND_CLANG_WARNINGS_C})
  list(APPEND C_WARNING_FLAGS_TO_CHECK ${CLANG_ONLY_WARNINGS_C})
elseif ("${CMAKE_C_COMPILER_ID}" MATCHES "MSVC")
  set(MSVC_ONLY_WARNINGS_C "/W3 /WX")
else()
  message(WARNING "Unknown compiler")
endif()

foreach (flag ${CXX_WARNING_FLAGS_TO_CHECK})
  set(CMAKE_CXX_FLAGS "${flag} ${CMAKE_CXX_FLAGS}")
endforeach()
foreach (flag ${C_WARNING_FLAGS_TO_CHECK})
  set(CMAKE_C_FLAGS "${flag} ${CMAKE_C_FLAGS}")
endforeach()