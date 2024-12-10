cmake_minimum_required(VERSION 3.10)
project(DisplayImage)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Find the OpenCV library
find_package(OpenCV REQUIRED)

# Find the SDL2 library
find_package(SDL2 REQUIRED)

# Find the FFmpeg library
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFmpeg REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

# Add the dynamic library Stitcher from stitcher.cpp
add_library(Stitcher SHARED stitcher.cpp)

# Specify include directories for the Stitcher library
target_include_directories(Stitcher PRIVATE 
    ${FFmpeg_INCLUDE_DIRS}  
    ${OpenCV_INCLUDE_DIRS}  
    ${CMAKE_CURRENT_SOURCE_DIR}  # Include current source directory for headers
)

# Add the executable DisplayImage from player.cpp
add_executable(DisplayImage player.cpp)

# Specify include directories for the DisplayImage executable
target_include_directories(DisplayImage PRIVATE 
    ${FFmpeg_INCLUDE_DIRS}  
    ${OpenCV_INCLUDE_DIRS}  
    ${SDL2_INCLUDE_DIRS}  
    ${CMAKE_CURRENT_SOURCE_DIR}  # Include current source directory for headers
)

# Link libraries to the DisplayImage executable
target_link_libraries(DisplayImage 
    Stitcher  
    PkgConfig::FFmpeg  
    ${OpenCV_LIBS}  
    SDL2::SDL2  
)

# The following line is redundant and can be removed
# include_directories(${SDL2_INCLUDE_DIRS} ${FFmpeg_INCLUDE_DIRS})