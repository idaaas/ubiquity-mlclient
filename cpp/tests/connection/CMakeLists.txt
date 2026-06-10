add_executable(test_connection
    test_connection.cpp
    ../../src/components/connection.cpp
    $<TARGET_OBJECTS:aeron_driver>
)
target_compile_options(test_connection PRIVATE
    -Wno-error=unused-function
    -Wno-unused-function
)
target_include_directories(test_connection PUBLIC  
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/config/tests>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/ubiquity_client>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src>
    $<BUILD_INTERFACE:${aeron_SOURCE_DIR}/aeron-driver/src/main/c>
    $<BUILD_INTERFACE:${aeron_SOURCE_DIR}/aeron-client/src/main/c>
    $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
target_link_libraries(test_connection PRIVATE aeron bsd uuid xxhash)

