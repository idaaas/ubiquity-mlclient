add_executable(test_shared_state_store
    test_shared_state_store.cpp
    ../../src/components/shared_state_store.cpp
    )
target_compile_options(test_shared_state_store PRIVATE
    -Wno-error=unused-function
    -Wno-unused-function
)
target_include_directories(test_shared_state_store PUBLIC  
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src/config/tests>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include/ubiquity_client>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../src>
    $<BUILD_INTERFACE:${json_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)
target_link_libraries(test_shared_state_store PRIVATE xxhash ssl crypto Threads::Threads
)
find_package(Threads REQUIRED)
    