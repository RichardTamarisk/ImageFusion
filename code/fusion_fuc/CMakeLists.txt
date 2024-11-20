cmake_minimum_required(VERSION 2.8)
project(ImageStitching)

# 查找 OpenCV
find_package(OpenCV REQUIRED)
# 查找 SDL2
find_package(SDL2 REQUIRED)

# 添加库
add_library(stitcher SHARED src/stitcher.cpp)

# 添加可执行文件（可选，仅用于测试）
add_executable(main main.cpp)

# 目标链接 OpenCV 和 SDL2 库
target_link_libraries(stitcher ${OpenCV_LIBS} SDL2::SDL2)
target_link_libraries(main stitcher ${OpenCV_LIBS} SDL2::SDL2)

# 包含头文件目录
target_include_directories(stitcher PUBLIC include)