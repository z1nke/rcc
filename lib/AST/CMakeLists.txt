add_library(AST
  ASTContext.cpp
  ASTDumper.cpp
  Decl.cpp
  Stmt.cpp
  Type.cpp
)

set_target_properties(AST
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(AST
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)