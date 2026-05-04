add_library(Lexer
  Lexer.cpp
  Preprocessor.cpp
  Token.cpp
)

set_target_properties(Lexer
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(Lexer
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(Lexer
  PRIVATE
  Basic
)