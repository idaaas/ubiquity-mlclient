cmake_minimum_required(VERSION 3.22)

project(
  ubiquityClient
  VERSION 0.1
  DESCRIPTION "The Ubiquity go client to connect and use the Machine Learning clusters"
  LANGUAGES CXX)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS OFF)

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

set(CMAKE_SWIG_FLAGS)

find_package(SWIG REQUIRED)

include(UseSWIG)

if(SWIG_FOUND)
  message("Found SWIG ${SWIG_VERSION}")
else()
  message("No SWIG found.")
  return()
endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS -intgosize 64)
endif()

# ubiquity_client_static dependencies
find_library(LIBBSD
  NAMES bsd libbsd
  REQUIRED)
find_library(LIBUUID
  NAMES uuid libuuid libuuid.dll
  REQUIRED)
find_package(CURL REQUIRED)
# Adds CURL::libcurl

# Find Go
#find_package(Go COMPONENTS Development REQUIRED)

# import ubiquity static library
add_library(ubiquity_client_static STATIC IMPORTED)
set_target_properties(ubiquity_client_static PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libubiquity_client_static.a)
target_include_directories(ubiquity_client_static INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set_property(SOURCE ubiquityclient.i PROPERTY CPLUSPLUS ON)
#set_property(SOURCE UbiquityClient.i PROPERTY COMPILE_OPTIONS
#  -package ai.ubiquity)

swig_add_library(libubiquityclientwrapper
  TYPE STATIC
  LANGUAGE go
  OUTPUT_DIR output
  OUTFILE_DIR .
  SOURCES ubiquityclient.i)

set_target_properties(libubiquityclientwrapper PROPERTIES
  SWIG_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)

# required by SWIG 4.2 to compile: https://www.swig.org/Doc4.2/Java.html#Java
target_compile_options(libubiquityclientwrapper PRIVATE -fno-strict-aliasing)

target_link_libraries(libubiquityclientwrapper PRIVATE ubiquity_client_static curl bsd uuid ssl crypto)

# add custom code to complete the binding
add_custom_command(
  TARGET libubiquityclientwrapper POST_BUILD
  COMMAND chmod a+x ${CMAKE_CURRENT_SOURCE_DIR}/postbuild.sh
  # Swig 4.2.1 `%proxycode` directive varies in functionnality depending on the language, doing it by hand
  COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/postbuild.sh ${CMAKE_CURRENT_BINARY_DIR}/output/Ubiquity.go ${CMAKE_CURRENT_SOURCE_DIR}/proxycode.go
)
