cmake_minimum_required(VERSION 3.15)

if (NOT DEFINED RCC_VERSION_MAJOR)
  set(RCC_VERSION_MAJOR 0)
endif ()

if (NOT DEFINED RCC_VERSION_MINOR)
  set(RCC_VERSION_MINOR 1)
endif ()

if (NOT DEFINED RCC_VERSION_PATCH)
  set(RCC_VERSION_PATCH 0)
endif ()

if (NOT DEFINED RCC_SCM_COMMIT_ID)
  set(RCC_SCM_COMMIT_ID "")
endif ()

project(rcc
  VERSION ${RCC_VERSION_MAJOR}.${RCC_VERSION_MINOR}.${RCC_VERSION_PATCH}
  DESCRIPTION "A toy risc-v c compiler"
  LANGUAGES C CXX ASM
)

if (NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to Debug.")
  set(CMAKE_BUILD_TYPE Debug)
endif (NOT CMAKE_BUILD_TYPE)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(FATAL_ERROR "
No build type selected. You need to pass -DCMAKE_BUILD_TYPE=<type> in order to configure.
Available options are:
  * -DCMAKE_BUILD_TYPE=Release - For an optimized build with no assertions or debug info.
  * -DCMAKE_BUILD_TYPE=Debug - For an unoptimized build with assertions and debug info.
  * -DCMAKE_BUILD_TYPE=RelWithDebInfo - For an optimized build with no assertions but with debug info.
  * -DCMAKE_BUILD_TYPE=MinSizeRel - For a build optimized for size instead of speed."
)
endif ()

string(TOUPPER "${CMAKE_BUILD_TYPE}" UPCASE_CMAKE_BUILD_TYPE)
if (CMAKE_BUILD_TYPE AND
    NOT UPCASE_CMAKE_BUILD_TYPE MATCHES "^(DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
  message(FATAL_ERROR "Invalid value for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif ()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

if ((CMAKE_GENERATOR MATCHES "Visual Studio") AND
    (CMAKE_GENERATOR_TOOLSET STREQUAL ""))
  message(WARNING "Visual Studio generators use the x86 host compiler by "
                  "default, even for 64-bit targets. This can result in linker "
                  "instability and out of memory errors. To use the 64-bit "
                  "host compiler, pass -Thost=x64 on the CMake command line.")
endif()

set(CMAKE_C_STANDARD 17 CACHE STRING "C standard to conform to")
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)


set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

include (HandleOptions)

# Generate a CompilationDatabase (compile_commands.json file) for our build,
# for use by clang_complete, YouCompleteMe, etc.
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(RCC_ENABLE_ASSERTIONS "Enable assert" ON)

if (WIN32)
  if (MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/lib)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/lib)
  else ()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  endif ()
else ()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

option(USE_FOLDERS
  "Enable solution folders in Visual Studio. Disable for Express versions."
  ON
)

if (USE_FOLDERS)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif ()

# Tests
option(ENABLE_LIT_TESTS "Enable lit tests" ON)
option(ENABLE_UNIT_TESTS "Enable unit tests" OFF)

add_subdirectory(tools)

if (ENABLE_LIT_TESTS)
  set(LIT_TOOL_NAMES "llvm-lit" "lit")
  find_program(LIT_TOOL
    NAMES ${LIT_TOOL_NAMES}
    HINTS ${LLVM_TOOLS_BINARY_DIR}
    DOC "Path to lit tool"
  )

  set(LIT_ARGS
    "-v;-s"
    CACHE
    STRING
    "llvm-lit arguments"
  )

  if (NOT CMAKE_CFG_INTDIR STREQUAL ".")
    list(APPEND LIT_ARGS --param build_mode=${CMAKE_CFG_INTDIR})
  endif ()

  if ((NOT LIT_TOOL) OR (NOT EXISTS ${LIT_TOOL}))
    message(FATAL_ERROR "The llvm-lit tool is required for lit tests."
      "  ${LIT_TOOL_NAMES} is not be found."
      "  You should disable 'ENABLE_LIT_TESTS' or install llvm-lit tool."
    )
  else ()
    message(STATUS "Found llvm-lit tool: ${LIT_TOOL}")
  endif ()
  add_subdirectory(test)
endif ()