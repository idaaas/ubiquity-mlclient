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
  Rng.cpp
  debug.cpp)

SET(HEADERS
  ../include/ubiquity_client/Ubiquity.hpp
  ClientHandleImpl.hpp
  ClusterClient.hpp
  LoadBalancer.hpp
  NodeClient.hpp
  Rng.hpp
  debug.hpp)

add_library(ubiquity_client SHARED 
  ${SOURCE} ${HEADERS}
  $<TARGET_OBJECTS:aeron_client_shared> #aeron_client_shared is built in static, so we need to import it here
  $<TARGET_OBJECTS:aeron_driver> #aeron_driver is built in static, so we need to import it here
)

add_library(ubiquity_client_static STATIC
  ${SOURCE} ${HEADERS}
  $<TARGET_OBJECTS:aeron_client_shared>
  $<TARGET_OBJECTS:aeron_driver>)

target_include_directories(ubiquity_client PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>  
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib)
)

target_include_directories(ubiquity_client_static PUBLIC  
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${aeron_SOURCE_DIR}/aeron-client/src/main/cpp>
    $<BUILD_INTERFACE:${aeron_SOURCE_DIR}/aeron-driver/src/main/c>
    $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${xxhash_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
target_compile_features(ubiquity_client_static PRIVATE cxx_std_17)

target_compile_features(ubiquity_client PRIVATE cxx_std_17)
target_compile_options(ubiquity_client PRIVATE -fstrict-aliasing)

# avoid relocation errors
set_property(TARGET ubiquity_client PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ubiquity_client_static PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(ubiquity_client PRIVATE nlohmann_json xxhash curl)
target_link_libraries(ubiquity_client_static INTERFACE curl ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} bsd uuid)
