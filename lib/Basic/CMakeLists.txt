add_library(Basic
  Diagnostic.cpp
  SourceManager.cpp
  Unreachable.cpp
)

set_target_properties(Basic
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(Basic
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)