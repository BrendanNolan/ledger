################################################################################
# F E T C H   M I N E R   L I B R A R Y
################################################################################
cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
project(fetch-miner)

# CMake Configuration
include(${FETCH_ROOT_CMAKE_DIR}/BuildTools.cmake)

# Compiler Configuration
setup_compiler()

#-------------------------------------------------------------------------------
# Main Library Target
#-------------------------------------------------------------------------------

setup_library(fetch-miner)
target_link_libraries(fetch-miner INTERFACE fetch-core fetch-miner)

