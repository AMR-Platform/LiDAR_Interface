cmake_minimum_required(VERSION 3.5)
project(slam_lidar_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(lidar_reader
  src/lidar_reader.cpp
)
target_include_directories(lidar_reader PUBLIC
  ${PROJECT_SOURCE_DIR}/src
)

add_executable(main_app
  src/main.cpp
)
target_link_libraries(main_app lidar_reader)

# Raw packet reader for debugging
add_executable(read_packets
  src/read_packets.cpp
)
target_link_libraries(read_packets
  # no extra libs needed
)

