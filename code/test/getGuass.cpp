#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <map>

#define STEP 6
#define ABS(X) ((X)>0? X:(-(X)))
#define PI 3.1415926

/*
L(x, y, σ) = G（x, y, σ）* I（x, y）
G(x, y, σ) = 1/（2πσ²）e-(x²+y²)/2σ²
D(x, y,σ)  = [G(x, y,kσ) - G(x, y,σ)] * I(x, y)
           = L(x, y, kσ) - L(x, y,σ)
*/
std::vector<cv::Mat> computeScaleSpace(const cv::Mat& image, double sigma_min, double sigma_max, int num_scales)
{
    std::vector<cv::Mat> scale_space(num_scales);
    std::vector<cv::Mat> dog_space; // 差分高斯图像
    // 初始化高斯核
    std::vector<double> sigmas(num_scales);
    for (int i = 0; i < num_scales; i++) 
    {
        sigmas[i] = sigma_min * std::pow(sigma_max / sigma_min, static_cast<double>(i) / (num_scales - 1));
    }

    // 计算高斯模糊图像
    for (int i = 0; i < num_scales; i++) 
    {
        // cv::GaussianBlur(image, scale_space[i], cv::Size(), sigmas[i], sigmas[i]);
        double sigma = sigmas[i];
        int ksize = static_cast<int>(6 * sigma + 1); // 高斯核大小为 6σ + 1
        ksize = (ksize % 2 == 0) ? ksize + 1 : ksize; // 确保核大小为奇数

        cv::Mat kernel(ksize, ksize, CV_64F);
        double sum = 0.0;
        for (int x = -ksize / 2; x <= ksize / 2; x++) 
        {
            for (int y = -ksize / 2; y <= ksize / 2; y++) 
            {
                double val = std::exp(-((x * x + y * y) / (2.0 * sigma * sigma))) / (2 * M_PI * sigma * sigma);
                kernel.at<double>(y + ksize / 2, x + ksize / 2) = val;
                sum += val;
            }
        }

        // 归一化高斯核
        kernel /= sum;

        // 执行高斯卷积
        cv::Mat blurred;
        cv::filter2D(image, blurred, image.depth(), kernel);
        scale_space[i] = blurred;
    }

    // 计算差分高斯图像
    // D(x, y,σ) = L(x, y, kσ) - L(x, y,σ)
    for (int i = 1; i < num_scales; i++) 
    {
        cv::Mat dog = scale_space[i] - scale_space[i-1];
        dog_space.push_back(dog);
    }

    return dog_space;
}

int main(int argc, char** argv)
{
    // 加载图像
    cv::Mat image = cv::imread("l.png", cv::IMREAD_GRAYSCALE);

    // 设置参数
    double sigma_min = 0.8;
    double sigma_max = 2.0;
    int num_scales = 4;

    std::vector<cv::Mat> dog_space = computeScaleSpace(image, sigma_min, sigma_max, num_scales);

    for (int i = 0; i < dog_space.size(); i++) 
    {
        // cv::imshow("Difference_of_Gaussians_" + std::to_string(i), dog_space[i]);
        cv::imwrite("Difference_of_Gaussians_" + std::to_string(i) + ".jpg", dog_space[i]);
        // cv::waitKey(0);
    }
}