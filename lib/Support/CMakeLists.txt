add_library(Support
  Error.cpp
)

set_target_properties(Support
  PROPERTIES
  FOLDER "Libs"
)

target_include_directories(Support
  PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)