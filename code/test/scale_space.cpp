#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <map>

// 尺度空间定义
std::vector<cv::Mat> computeScaleSpace(const cv::Mat& image,
                                       double sigma_min, double sigma_max, int num_scales)
{
    std::vector<cv::Mat> scale_space;
    double sigma_step = std::pow(sigma_max / sigma_min, 1.0 / (num_scales - 1));

    // 遍历所需的尺度数量
    for(int i = 0; i < num_scales; i++)
    {
        // 计算当前尺度的sigma值
        double sigma = sigma_min * std::pow(sigma_step, i);

        // 创建一个新的Mat来存储模糊后的图像
        cv::Mat blurred;

        // 使用当前sigma值对输入图像进行高斯模糊
        cv::GaussianBlur(image, blurred, cv::Size(), sigma);

        // 将模糊后的图像添加到尺度空间向量中
        scale_space.push_back(blurred);
    }

    return scale_space;
}

// 可以等步长模糊图像
int main(int argc, char **argv)
{
    cv::Mat input_image = cv::imread("l.png");

    double sigma_min = 0.5;
    double sigma_max = 2.0;
    int num_scales = 5;

    std::vector<cv::Mat> scale_space = computeScaleSpace(input_image, sigma_min, sigma_max, num_scales);

    for(int i = 0; i < num_scales; i++)
    {
        std::stringstream filename;
        filename << "Scale_" << i << ".png";
        cv::imwrite(filename.str(), scale_space[i]);
    }

    return 0;
}