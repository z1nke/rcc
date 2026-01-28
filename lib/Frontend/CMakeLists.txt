add_library(Frontend
  CompilerInstance.cpp
  CompilerInvocation.cpp
)

set_target_properties(Frontend
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(Frontend
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)
