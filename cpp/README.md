# C++ ML Client

```bash
earthly +build --lang=cpp --os=ubuntu:22.04
ls local-ouput/cpp/ubuntu:22.04
# Ubiquity.hpp: the header file to include
# libubiquity_client_static.a: the static library
# html: the Doxygen documentation
```

## Compiling on the CLI

```
/usr/bin/c++  -IUBIQUITY_HPP_PATH -std=c++11 -o main.cpp.o -c main.cpp
/usr/bin/c++ -std=c++11 main.cpp.o -o main libubiquity_client_static.a -lcurl -lpthread -ldl -lbsd -luuid
```

## CMake 3.22 or higher

```cmake
include(FetchContent)
FetchContent_Declare(ubiquity
  GIT_REPOSITORY https://github.com/idaaas/ubiquity-client
  GIT_TAG xxx
  SOURCE_SUBDIR cpp)
FetchContent_MakeAvailable(ubiquity)

add_executable(main ${SOURCE})

target_compile_features(main PRIVATE cxx_std_11)

target_link_libraries(main PRIVATE ubiquity_client_static) 
```
## Running Examples

Cpp examples are provided in a docker to quickly run scenarios:
```bash
earthly +examples --lang=cpp --os=ubuntu:22.04
docker run cpp-examples ./line-item API_TOKEN CLUSTER_ID
```