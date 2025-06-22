cmake_minimum_required(VERSION 3.13)

# ============================================================================================
# Constants
# ============================================================================================
string(APPEND TEST_EXECUTABLE_NAME "${APPLICATION_NAME}_tests")

# ============================================================================================
# Packages
# ============================================================================================

#Originally wanted to use #pkg_check_modules(GTEST REQUIRED IMPORTED_TARGET gtest) but got undefined refernce issues
#TODO: Could possibly do the download and install gtest features here, but not sure if its worth it at this time

find_package(GTest REQUIRED)

# ============================================================================================
# Declare Executable, This creates targets
# ============================================================================================

add_executable(${TEST_EXECUTABLE_NAME} main.cpp
    address_claimer_test.cpp
    frame_test.cpp
    header_test.cpp
    state_machine_test.cpp
    network_test.cpp
    network_process_test.cpp
    name_test.cpp
    transport_protocol_test.cpp
)

#
add_test(NAME test COMMAND ${TEST_EXECUTABLE_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/ )


# ============================================================================================
# Includes
# ============================================================================================

target_include_directories(${TEST_EXECUTABLE_NAME} 
  PUBLIC 
  ${CMAKE_SOURCE_DIR}/${INCLUDE_DIR}
  ${GTEST_INCLUDE_DIRS}
)

# ============================================================================================
# Linking
# ============================================================================================
target_link_libraries(${TEST_EXECUTABLE_NAME} 
  PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  ${GTEST_LIBRARIES}
  jay::jay
)
