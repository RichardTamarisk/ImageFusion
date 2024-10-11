cmake_minimum_required(VERSION 3.10)
project(DisplayImage)

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
)

# Add the executable
add_executable(DisplayImage player.cpp)

# Link the libraries to the executable
target_link_libraries(DisplayImage 
    PkgConfig::FFmpeg
    ${OpenCV_LIBS}  # 使用 OpenCV_LIBS 代替 OpenCV::OpenCV
    SDL2::SDL2
)

# Include the directories for the libraries
include_directories(${SDL2_INCLUDE_DIRS} ${FFmpeg_INCLUDE_DIRS})