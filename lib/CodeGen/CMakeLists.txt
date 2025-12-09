add_library(CodeGen
  CodeGen.cpp
)

set_target_properties(CodeGen
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(CodeGen
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)