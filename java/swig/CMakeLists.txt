cmake_minimum_required(VERSION 3.22)

project(
  jniUbiquityClient
  VERSION 0.1
  DESCRIPTION "The Ubiquity java client to connect and use the Machine Learning clusters"
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

if(${SWIG_VERSION} VERSION_GREATER_EQUAL 4)
  list(APPEND CMAKE_SWIG_FLAGS "-doxygen")
endif()

if(UNIX AND NOT APPLE)
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
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

# Find Java and JNI
find_package(Java COMPONENTS Development REQUIRED)
find_package(JNI REQUIRED)

# import ubiquity static library
add_library(ubiquity_client_static STATIC IMPORTED)
set_target_properties(ubiquity_client_static PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libubiquity_client_static.a)
target_include_directories(ubiquity_client_static INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set_property(SOURCE UbiquityClient.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE UbiquityClient.i PROPERTY COMPILE_OPTIONS
  -package ai.ubiquity)

swig_add_library(jniUbiquityClient
  TYPE SHARED
  LANGUAGE java
  OUTPUT_DIR output
  OUTFILE_DIR .
  SOURCES UbiquityClient.i)

set_target_properties(jniUbiquityClient PROPERTIES
  SWIG_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)

target_include_directories(jniUbiquityClient PRIVATE ${JNI_INCLUDE_DIRS})

# required by SWIG 4.2 to compile: https://www.swig.org/Doc4.2/Java.html#Java
target_compile_options(jniUbiquityClient PRIVATE -fno-strict-aliasing)

target_link_libraries(jniUbiquityClient PRIVATE ubiquity_client_static curl bsd uuid)

# add custom code to complete the binding
add_custom_command(
  TARGET jniUbiquityClient POST_BUILD
  # the native loader
  COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/NativeLoader.java ${CMAKE_CURRENT_BINARY_DIR}/output/NativeLoader.java
  # Swig 4.2.1 `%proxycode` directive varies in functionnality depending on the language, doing it by hand
  COMMAND sed -i '/public class Ubiquity {/r ${CMAKE_CURRENT_SOURCE_DIR}/proxycode.java' ${CMAKE_CURRENT_BINARY_DIR}/output/Ubiquity.java
)
