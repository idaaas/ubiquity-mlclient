SET(SOURCE
  line-item/Main.cpp)

SET(HEADERS
  ../include/ubiquity_client/Ubiquity.hpp
  common/ApiUtil.hpp
  common/FileUtil.hpp)

add_executable(line-item ${SOURCE})

target_link_libraries(line-item PRIVATE ubiquity_client_static)
