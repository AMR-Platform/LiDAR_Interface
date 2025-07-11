cmake_minimum_required(VERSION 3.10)
project(LidarMSOPReader)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler flags for debugging
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create the main executable
add_executable(lidar_reader
    main.cpp
    msop_parser.cpp
)

# Create the data collector/visualizer executable
add_executable(lidar_visualizer
    lidar_visualizer.cpp
    msop_parser.cpp
)

# Create the angle calculation test executable
add_executable(test_angle_calculation
    test_angle_calculation.cpp
    msop_parser.cpp
)

# Link libraries for all executables
target_link_libraries(lidar_reader
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(lidar_visualizer
    ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(test_angle_calculation
    ${CMAKE_THREAD_LIBS_INIT}
)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ flags (Debug): ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "C++ flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
