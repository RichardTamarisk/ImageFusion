#include "include/stitcher.h"

int main(int argc, char** argv) {
    // 读取两张 JPEG 图片
    cv::Mat img1 = cv::imread("../img/left_1.jpg");
    cv::Mat img2 = cv::imread("../img/right_1.jpg");

    // 检查图像是否成功读取
    if (img1.empty() || img2.empty()) {
        std::cerr << "无法读取图像文件" << std::endl;
        return -1;
    }

    // // 确保图像深度为 CV_8U
    // if (img1.depth() != CV_8U) {
    //     img1.convertTo(img1, CV_8U);
    // }
    // if (img2.depth() != CV_8U) {
    //     img2.convertTo(img2, CV_8U);
    // }

    // 将 cv::Mat 转换为 AVFrame
    AVFrame *frame1 = cvmatToAvframe(&img1, nullptr);
    AVFrame *frame2 = cvmatToAvframe(&img2, nullptr);

    // 检查 AVFrame 是否成功分配
    if (!frame1 || !frame2) {
        std::cerr << "无法分配 AVFrame" << std::endl;
        return -1;
    }

    // 创建输出帧
    AVFrame *frame_fused = av_frame_alloc();
    if (!frame_fused) {
        std::cerr << "无法分配输出帧" << std::endl;
        return -1;
    }

    // 调用 image_fusion 函数
    bool success = image_fusion(frame1, frame2, frame_fused, false);
    if (success) {
        std::cout << "图像拼接成功！" << std::endl;
        // 处理输出帧，如保存或显示
    } else {
        std::cerr << "图像拼接失败！" << std::endl;
    }

    // 释放资源
    av_frame_free(&frame1);
    av_frame_free(&frame2);
    av_frame_free(&frame_fused);

    return 0;
}