if (DISABLE_ID_CHECKS)
  add_definitions(-DDISABLE_ID_CHECKS)
endif (DISABLE_ID_CHECKS)

SET(SOURCE
  ClientHandle.cpp
  ClientHandleImpl.cpp
  ClusterClient.cpp
  LoadBalancer.cpp
  NodeClient.cpp
  Ubiquity.cpp
  debug.cpp)

SET(HEADERS
  ../include/ubiquity_client/Ubiquity.hpp
  ClientHandleImpl.hpp
  ClusterClient.hpp
  LoadBalancer.hpp
  NodeClient.hpp
  debug.hpp)

add_library(ubiquity_client SHARED ${SOURCE} ${HEADERS})
add_library(ubiquity_client_static STATIC
  $<TARGET_OBJECTS:ubiquity_client>
  $<TARGET_OBJECTS:aeron_client_shared>
  $<TARGET_OBJECTS:aeron_driver>)

target_include_directories(ubiquity_client PUBLIC ../include)
target_include_directories(ubiquity_client_static PUBLIC ../include)

target_compile_features(ubiquity_client PRIVATE cxx_std_17)

target_compile_options(ubiquity_client PRIVATE -fstrict-aliasing)

target_link_libraries(ubiquity_client PUBLIC aeron_client_shared aeron_driver nlohmann_json xxhash curl)
target_link_libraries(ubiquity_client_static INTERFACE curl ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} bsd uuid)
