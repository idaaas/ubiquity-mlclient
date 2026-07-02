cmake_minimum_required(VERSION 3.22)

project(
  CsharpBindings
  VERSION 0.1
  DESCRIPTION "The csharp client to connect and use the Machine Learning clusters"
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

if(UNIX AND NOT APPLE) # à valider qu'actif pour csharp
  list(APPEND CMAKE_SWIG_FLAGS "-DSWIGWORDSIZE64")
endif()

# mlclient_client_static dependencies
find_library(LIBBSD
  NAMES bsd libbsd
  REQUIRED)
find_library(LIBUUID
  NAMES uuid libuuid libuuid.dll
  REQUIRED)
find_package(CURL REQUIRED)
# Adds CURL::libcurl

# import mlclient static library
add_library(ubiquity_client_static STATIC IMPORTED)
set_target_properties(ubiquity_client_static PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libubiquity_client_static.a)
target_include_directories(ubiquity_client_static INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set_property(SOURCE interface.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE interface.i PROPERTY COMPILE_OPTIONS
  -namespace ai.ubiquity
  -dllimport mlclient
)

swig_add_library(mlclient
  TYPE SHARED
  LANGUAGE csharp
  OUTPUT_DIR output
  OUTFILE_DIR .
  SOURCES interface.i)

set_target_properties(mlclient PROPERTIES
  SWIG_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON
  POSITION_INDEPENDENT_CODE ON)

# required by SWIG 4.2 to compile: https://www.swig.org/Doc4.2/Java.html#Java
target_compile_options(mlclient PRIVATE -fno-strict-aliasing)

target_link_libraries(mlclient PRIVATE ubiquity_client_static curl bsd uuid)

# Detect RID for packaging folder
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
  set(DETECTED_RID "win-x64")
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  set(DETECTED_RID "osx-x64")
elseif(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  set(DETECTED_RID "linux-x64")
else()
  message(FATAL_ERROR "Unsupported platform/arch for the lib packaging")
endif()

set(PACKAGE_NATIVE_DIR "${CMAKE_SOURCE_DIR}/package/native/${DETECTED_RID}")

file(MAKE_DIRECTORY "${PACKAGE_NATIVE_DIR}")

# Copy the built native library into the package staging RID folder.
add_custom_command(
  TARGET mlclient POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:mlclient> "${PACKAGE_NATIVE_DIR}/$<TARGET_FILE_NAME:mlclient>"
)

# Add custom code to complete the binding
add_custom_command(
  TARGET mlclient POST_BUILD
  # Swig 4.x `%proxycode` directive varies in functionnality depending on the language, doing it by hand
  COMMAND sed -i '/public class Ubiquity {/r ${CMAKE_CURRENT_SOURCE_DIR}/proxycode.csharp' ${CMAKE_CURRENT_BINARY_DIR}/output/Ubiquity.cs
)
