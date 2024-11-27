cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(ImageFusion)

# 设置 FFmpeg 的库和包含路径
set(FFMPEG_INCLUDE_DIRS /usr/local/include)
set(FFMPEG_LIBRARIES /usr/local/lib)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 查找 SDL2
find_package(SDL2 REQUIRED)

# 添加可执行文件
add_executable(DisplayImage main.cpp src/stitcher.cpp)

# 指定包含目录
target_include_directories(DisplayImage PRIVATE 
    ${FFMPEG_INCLUDE_DIRS}  # FFmpeg 包含目录
    ${OpenCV_INCLUDE_DIRS}   # OpenCV 包含目录
    ${SDL2_INCLUDE_DIRS}     # SDL2 包含目录
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # 自定义头文件目录
)

# 链接库
target_link_libraries(DisplayImage 
    PRIVATE 
    ${FFMPEG_LIBRARIES}/libavcodec.so 
    ${FFMPEG_LIBRARIES}/libavformat.so 
    ${FFMPEG_LIBRARIES}/libavutil.so 
    ${FFMPEG_LIBRARIES}/libswscale.so 
    ${OpenCV_LIBS} 
    SDL2::SDL2
)