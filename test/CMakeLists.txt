add_custom_target(run-test
  COMMAND ${CMAKE_COMMAND} -E echo "== Running tests =="
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/driver_test.sh ${CMAKE_BINARY_DIR}/
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/simple_test.sh ${CMAKE_BINARY_DIR}/
  COMMAND ${CMAKE_BINARY_DIR}/driver_test.sh
  COMMAND ${CMAKE_BINARY_DIR}/simple_test.sh
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
)

add_dependencies(run-test rcc)