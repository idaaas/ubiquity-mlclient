add_executable(test_client_handle
    test_client_handle.cpp
    ../../src/components/client_handle.cpp
    )
target_compile_options(test_client_handle PRIVATE
    -Wno-error=unused-function
    -Wno-unused-function
    -Wno-vla
)
target_include_directories(test_client_handle PUBLIC  
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/config/tests>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/ubiquity_client>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src>
    $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
target_link_libraries(test_client_handle PRIVATE xxhash)
