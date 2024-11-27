cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(ImageFusion)

# 设置 FFmpeg 的库和包含路径
set(FFMPEG_INCLUDE_DIRS /usr/local/include)
set(FFMPEG_LIBRARIES /usr/local/lib)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 添加动态库
add_library(Stitcher SHARED src/stitcher.cpp)

# 添加可执行文件
add_executable(DisplayImage main.cpp)

# 指定包含目录
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
target_link_libraries(Stitcher 
    PRIVATE 
    ${FFMPEG_LIBRARIES}/libavcodec.so 
    ${FFMPEG_LIBRARIES}/libavformat.so 
    ${FFMPEG_LIBRARIES}/libavutil.so 
    ${FFMPEG_LIBRARIES}/libswscale.so 
)

target_link_libraries(DisplayImage 
    PRIVATE 
    Stitcher  # 链接到动态库
    ${OpenCV_LIBS} 
)
