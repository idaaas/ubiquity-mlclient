if (DISABLE_ID_CHECKS)
  add_definitions(-DDISABLE_ID_CHECKS)
endif (DISABLE_ID_CHECKS)

# Get the latest abbreviated commit hash of the working branch
#execute_process(
#  COMMAND git log -1 --format=%h
#  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#  OUTPUT_VARIABLE GIT_HASH
#  OUTPUT_STRIP_TRAILING_WHITESPACE
#  )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/GitVersion.cpp.in ${CMAKE_CURRENT_SOURCE_DIR}/GitVersion.cpp @ONLY)

SET(SOURCE
    GitVersion.cpp
    client_handle_wrapper.cpp
    components/client_handle.cpp
    components/cluster_client.cpp
    components/connection.cpp
    components/idle_strategy.cpp
    components/node_client.cpp
    components/synchronizer.cpp
    components/shared_state_store.cpp
    components/load_balancer.cpp
    components/ubiquity.cpp
    components/web_client.cpp
    ubiquity_wrapper.cpp
    utils/logger.cpp
    )

SET(HEADERS
    ../include/ubiquity_client/Ubiquity.hpp
    GitVersion.hpp
    components/client_handle.hpp
    components/cluster_client.hpp
    components/connection.hpp
    components/idle_strategy.hpp
    components/node_client.hpp
    components/synchronizer.hpp
    components/shared_state_store.hpp
    components/load_balancer.hpp
    utils/prng.hpp
    utils/logger.hpp
    )

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/.>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/config/release>
    $<BUILD_INTERFACE:${aeron_SOURCE_DIR}/aeron-driver/src/main/c>
    $<BUILD_INTERFACE:${aeron_SOURCE_DIR}/aeron-client/src/main/c>
    $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${xxhash_SOURCE_DIR}>
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
target_compile_features(ubiquity_client_static PRIVATE cxx_std_17)

target_compile_features(ubiquity_client PRIVATE cxx_std_17)
target_compile_options(ubiquity_client PRIVATE -fstrict-aliasing)

# avoid relocation errors
set_property(TARGET ubiquity_client PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ubiquity_client_static PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(ubiquity_client PRIVATE nlohmann_json xxhash curl ssl crypto)
target_link_libraries(ubiquity_client_static INTERFACE curl ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} bsd uuid ssl crypto)
