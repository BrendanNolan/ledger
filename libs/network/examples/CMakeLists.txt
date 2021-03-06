################################################################################
# F E T C H   N E T W O R K   E X A M P L E S
################################################################################
cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(fetch-network)

# CMake configuration
include(${FETCH_ROOT_CMAKE_DIR}/BuildTools.cmake)

# Compiler Configuration
setup_compiler()

#-------------------------------------------------------------------------------
# Example Targets
#-------------------------------------------------------------------------------

setup_library_examples(fetch-network)

if(FETCH_ENABLE_EXAMPLES)

  target_link_libraries(example_http_server PRIVATE fetch-http)

  add_executable(example_subscribe subscribe/subscribe.cpp)
  target_link_libraries(example_subscribe PRIVATE fetch-network)
  target_include_directories(example_subscribe PRIVATE subscribe)

  add_executable(example_listener subscribe/listener.cpp)
  target_link_libraries(example_listener PRIVATE fetch-network)
  target_include_directories(example_listener PRIVATE subscribe)

  add_executable(example_rpc_server rpc/server.cpp)
  target_link_libraries(example_rpc_server PRIVATE fetch-network)

  add_executable(example_rpc_client rpc/client.cpp)
  target_link_libraries(example_rpc_client PRIVATE fetch-network)

  add_executable(example_authentication_protocol_client authentication_protocol/client.cpp)
  target_link_libraries(example_authentication_protocol_client PRIVATE fetch-network)

  add_executable(example_authentication_protocol_server authentication_protocol/server.cpp)
  target_link_libraries(example_authentication_protocol_server PRIVATE fetch-network)

  add_executable(example_adv_rpc_aea advanced_rpc/aea.cpp)
  target_link_libraries(example_adv_rpc_aea PRIVATE fetch-network)

  add_executable(example_adv_rpc_node advanced_rpc/node.cpp)
  target_link_libraries(example_adv_rpc_node PRIVATE fetch-network)

  add_executable(example_rrpc_client reverse_rpc/client.cpp)
  target_link_libraries(example_rrpc_client PRIVATE fetch-network)

  add_executable(example_rrpc_server reverse_rpc/server.cpp)
  target_link_libraries(example_rrpc_server PRIVATE fetch-network)

endif(FETCH_ENABLE_EXAMPLES)

