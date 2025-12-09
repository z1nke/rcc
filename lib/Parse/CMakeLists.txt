add_library(Parser
  Parser.cpp
)

set_target_properties(Parser
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(Parser
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(Parser
  PRIVATE
  AST
  Basic
  Lexer
)