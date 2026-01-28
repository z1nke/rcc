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