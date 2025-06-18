#
# Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no), 2025 Hong.Guo (hong.guo@advantech.com.cn)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/guoh27/jay
#

cmake_minimum_required(VERSION 3.13)

# ============================================================================================
# Constants
# ============================================================================================
string(APPEND APPLICATION_NAME "jay")
string(APPEND INCLUDE_DIR "include")


# ============================================================================================
# Project Description
# ============================================================================================
project(jay VERSION 0.2 DESCRIPTION "J1939 library" LANGUAGES CXX)

# ============================================================================================
# Build Options
# ============================================================================================

#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(THREADS_PREFER_PTHREAD_FLAG "Prefere pthread threads" ON)


option(JAY_BUILD_TESTING "Build jay tests." ON)
option(JAY_BUILD_EXAMPLES "Build examples." ON)
option(JAY_BUILD_DOCS "Build jay documentation." OFF)

# ============================================================================================
# Libraries
# ============================================================================================


#Fing Canary
if(NOT TARGET canary::canary)
  find_package(canary QUIET)
  if(NOT canary_FOUND)
    message(STATUS "Canary not found, trying to find it in the system")
    pkg_check_modules(canary REQUIRED IMPORTED_TARGET canary)
  endif()
endif()

if(NOT TARGET canary::canary)
  message(FATAL_ERROR "Canary not found, please install it or build it from source")
endif()

#Find Boost
find_package(Boost COMPONENTS system REQUIRED CONFIG) # header only libraries must not be added here

#Find threads
find_package(Threads)

# ============================================================================================
# Declare Executable, This creates targets
# ============================================================================================

add_library(${APPLICATION_NAME} INTERFACE)
add_library(jay::jay ALIAS ${APPLICATION_NAME})

target_include_directories(${APPLICATION_NAME} INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(
  ${APPLICATION_NAME}
    INTERFACE
      Boost::system
      canary::canary
      Threads::Threads
)

# link sml when use cmake
if(TARGET sml::sml)
  target_link_libraries(
    ${APPLICATION_NAME}
      INTERFACE
        sml::sml
  )
endif()

target_compile_features(${APPLICATION_NAME} INTERFACE cxx_std_17)

# =============================================
# Build Tests
# =============================================

#include(CTest)
if(JAY_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# =============================================
# Build Examples
# =============================================
if(JAY_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# =============================================
# Build Doc
# =============================================
if (JAY_BUILD_DOCS)
  file(GLOB JAY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR}/${APPLICATION_NAME}/*.hpp")
  find_package(standardese REQUIRED)
  standardese_generate(jay_docs
    INCLUDE_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR}
      ${Boost_INCLUDE_DIR}
    INPUT ${JAY_HEADERS})
endif()

# =============================================
# Install Target
# =============================================

include(GNUInstallDirs)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${INCLUDE_DIR}/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

install(TARGETS ${APPLICATION_NAME}
  EXPORT jayTargets
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(jay_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})
unset(CMAKE_SIZEOF_VOID_P)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "jayConfigVersion.cmake"
  COMPATIBILITY AnyNewerVersion
)

set(CMAKE_SIZEOF_VOID_P ${jay_SIZEOF_VOID_P})
unset(jay_SIZEOF_VOID_P)

install(FILES
  "jayConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/jayConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${APPLICATION_NAME}
)

install(EXPORT jayTargets
  FILE jayTargets.cmake
  NAMESPACE ${APPLICATION_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${APPLICATION_NAME}
)
