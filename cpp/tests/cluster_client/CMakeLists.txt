add_executable(test_cluster_client
    test_cluster_client.cpp
    ../../src/components/cluster_client.cpp
    )
target_compile_options(test_cluster_client PRIVATE
    -Wno-error=unused-function
    -Wno-unused-function
)
target_include_directories(test_cluster_client PUBLIC  
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/config/tests>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/ubiquity_client>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src>
    $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
target_link_libraries(test_cluster_client PRIVATE xxhash)
