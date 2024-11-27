cmake_minimum_required(VERSION 3.10)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 设置项目名称
project(ImageFusion)

# 查找 FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 添加动态库
add_library(Stitcher SHARED src/stitcher.cpp)
# 添加动态库
add_library(Stitcher SHARED src/stitcher.cpp)

# 添加可执行文件
add_executable(DisplayImage main.cpp)
add_executable(DisplayImage main.cpp)

# 指定包含目录
target_include_directories(Stitcher PRIVATE 
target_include_directories(Stitcher PRIVATE 
    ${FFMPEG_INCLUDE_DIRS}  # FFmpeg 包含目录
    ${OpenCV_INCLUDE_DIRS}   # OpenCV 包含目录
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # 自定义头文件目录
)

target_include_directories(DisplayImage PRIVATE 
    ${FFMPEG_INCLUDE_DIRS}  
    ${OpenCV_INCLUDE_DIRS}   
    ${CMAKE_CURRENT_SOURCE_DIR}/include  
)

# 链接库
target_link_libraries(Stitcher PRIVATE 
    PkgConfig::FFMPEG 
)

target_link_libraries(DisplayImage PRIVATE 
    Stitcher  # 链接到动态库
    PkgConfig::FFMPEG  # 确保链接 FFmpeg 库
    ${OpenCV_LIBS} 
)
