configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
  @ONLY
)

add_custom_target(run-test
  COMMAND ${LIT_TOOL} ${LIT_ARGS} ${CMAKE_CURRENT_BINARY_DIR}/units
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
)

add_dependencies(run-test rcc)