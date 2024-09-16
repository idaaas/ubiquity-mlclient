cmake_minimum_required(VERSION 3.22)

project(
  ubiquity_client
  VERSION 0.1
  DESCRIPTION "The Ubiquity client to connect and use the Machine Learning clusters"
  LANGUAGES CXX)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)

  # Let's ensure -std=c++xx instead of -std=g++xx
  set(CMAKE_CXX_EXTENSIONS OFF)

  option(DISABLE_ID_CHECKS "Enable faster Crockford's id decoding when ids only come from the REST API and are safe" OFF)

  if (UNIX)
    add_compile_options(-Werror -Wall -Wpedantic -Wextra -Wno-unused-parameter -Wno-address-of-packed-member -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer)
  endif()

  # Let's nicely support folders in IDEs
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  include(CTest)

  # Docs only available if this is the main app
  find_package(Doxygen)
  if(Doxygen_FOUND)
    add_subdirectory(docs)
  else()
    message(STATUS "Doxygen not found, not building docs")
  endif()
endif()

find_package(Threads REQUIRED)

find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(aeron
  GIT_REPOSITORY https://github.com/real-logic/aeron.git
  GIT_TAG 1.43.0)
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)
FetchContent_Declare(xxHash
  GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
  GIT_TAG v0.8.2
  SOURCE_SUBDIR cmake_unofficial)
FetchContent_MakeAvailable(aeron json xxHash)
# Adds aeron_driver aeron_client nlohmann_json xxhash

# The compiled library code is here
add_subdirectory(src)

if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR MODERN_CMAKE_BUILD_TESTING)
   AND BUILD_TESTING)
  add_subdirectory(tests)
endif()
