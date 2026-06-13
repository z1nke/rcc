add_executable(rcc
  rcc.cpp
)

set_target_properties(rcc
  PROPERTIES
  FOLDER "Tools"
)

target_include_directories(rcc
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(rcc
  PRIVATE
  -Wl,--start-group
  AST
  Basic
  CodeGen
  Frontend
  Lexer
  Parser
  Sema
  Support
  -Wl,--end-group
)

# Freestanding C headers.
set(RCC_RESOURCE_INCLUDE_DIR ${CMAKE_BINARY_DIR}/lib/rcc/include)
set(RCC_C_HEADERS
  float.h
  stdalign.h
  stdarg.h
  stdbool.h
  stddef.h
  stdnoreturn.h
)

set(RCC_C_HEADER_SOURCES)
foreach(Header ${RCC_C_HEADERS})
  list(APPEND RCC_C_HEADER_SOURCES
       ${CMAKE_SOURCE_DIR}/lib/rcc/include/${Header})
endforeach()

add_custom_command(
  TARGET rcc
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${RCC_RESOURCE_INCLUDE_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${RCC_C_HEADER_SOURCES}
          ${RCC_RESOURCE_INCLUDE_DIR}
  COMMENT "Copying freestanding C headers to lib/rcc/include"
)
