add_library(Sema
  DeclSpec.cpp
  Sema.cpp
)

set_target_properties(Sema
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(Sema
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(Sema
  PRIVATE
  AST
  Basic
  Lexer
)