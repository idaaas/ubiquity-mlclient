SET(SOURCE_LINE_ITEM
  line-item/Main.cpp)

SET(SOURCE_ACTION_RATE
  action-rate/Main.cpp)

SET(SOURCE_PROBABILITY_ESTIMATOR
  probability-estimator/Main.cpp)

SET(SOURCE_BINARY_CLASSIFICATION
  binary-classification/Main.cpp)

SET(HEADERS
  ../include/ubiquity_client/Ubiquity.hpp
  common/ApiUtil.hpp
  common/FileUtil.hpp)

add_executable(line-item ${SOURCE_LINE_ITEM})
add_executable(action-rate ${SOURCE_ACTION_RATE})
add_executable(probability-estimator ${SOURCE_PROBABILITY_ESTIMATOR})
add_executable(binary-classification ${SOURCE_BINARY_CLASSIFICATION})

target_link_libraries(line-item PRIVATE ubiquity_client_static)
target_link_libraries(action-rate PRIVATE ubiquity_client_static)
target_link_libraries(probability-estimator PRIVATE ubiquity_client_static)
target_link_libraries(binary-classification PRIVATE ubiquity_client_static)
