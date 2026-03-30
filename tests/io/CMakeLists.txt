# This test needs test instance files, so we copy them to the corresponding build dir
file(COPY test_instances DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_gmock_test(hypergraph_io_test hypergraph_io_test.cc)
add_custom_command(TARGET hypergraph_io_test PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/test_instances
          ${CMAKE_CURRENT_BINARY_DIR}/test_instances)
