#include "stitcher.h"
#include <iostream>

int main(int argc, char** argv) {
    const char* img1_path = "../img/left_1.jpg";  // 输入左图像路径
    const char* img2_path = "../img/right_1.jpg"; // 输入右图像路径
    const char* output_path = "../img/result.jpg"; // 输出拼接结果路径

    if (stitchImages(img1_path, img2_path, output_path)) {
        std::cout << "Image stitching completed." << std::endl;
    } else {
        std::cerr << "Image stitching failed." << std::endl;
    }

    return 0;
}