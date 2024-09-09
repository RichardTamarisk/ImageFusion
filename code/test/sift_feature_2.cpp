#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <map>

std::vector<cv::Mat> computeScaleSpace(const cv::Mat& image,
                                       double sigma_min, double sigma_max, int num_scales) 
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
        cv::GaussianBlur(image, scale_space[i], cv::Size(), sigmas[i], sigmas[i]);
    }

    // 计算差分高斯图像
    for (int i = 1; i < num_scales; i++) 
    {
        cv::Mat dog = scale_space[i] - scale_space[i-1];
        dog_space.push_back(dog);
    }

    return dog_space;
}

// 计算Harris角点响应值
float computeCornerResponse(const cv::Mat& dog, int x, int y, int s) 
{
    float Ixx = dog.at<float>(y-1,x) - 2*dog.at<float>(y,x) + dog.at<float>(y+1,x);
    float Iyy = dog.at<float>(y,x-1) - 2*dog.at<float>(y,x) + dog.at<float>(y,x+1);
    float Ixy = (dog.at<float>(y-1,x-1) - dog.at<float>(y-1,x+1) -
                dog.at<float>(y+1,x-1) + dog.at<float>(y+1,x+1)) / 4.0f;
    
    float cornerness = Ixx * Iyy - Ixy * Ixy - 0.04f * (Ixx + Iyy) * (Ixx + Iyy);
    return cornerness;
}

std::vector<cv::KeyPoint> detectKeypoints(const std::vector<cv::Mat>& dog_space) 
{
    std::vector<cv::KeyPoint> keypoints;

    printf("start detect keypoints\n");
    for (int s = 0; s < dog_space.size(); s++) 
    {
        printf("1 for\n");
        int rows = dog_space[s].rows;
        int cols = dog_space[s].cols;
        for (int y = 1; y < rows - 1; y++) 
        {
            printf("2 for\n");
            for (int x = 1; x < cols - 1; x++) 
            {
                printf("3 for %d\n", x);
                // 检查当前点是否为局部极值
                float center = dog_space[s].at<float>(y,x);
                if (std::abs(center) < 0.03) 
                    continue; // 忽略幅值太小的点

                printf("here\n");
                bool is_extrema = true;
                for (int dy = -1; dy <= 1; dy++) 
                {
                    printf("4 for\n");
                    for (int dx = -1; dx <= 1; dx++) 
                    {
                        printf("5 for\n");
                        if (dy == 0 && dx == 0) continue;
                        int y_neighbor = y + dy;
                        int x_neighbor = x + dx;
                        if (y_neighbor < 0 || y_neighbor >= rows || x_neighbor < 0 || x_neighbor >= cols) 
                        {
                            continue; // 跳过超出图像边界的邻域点
                        }
                        float neighbor = dog_space[s].at<float>(y_neighbor, x_neighbor);
                        if (std::abs(center) <= std::abs(neighbor)) 
                        {
                            is_extrema = false;
                            break;
                        }
                    }
                    if (!is_extrema) break;
                }

                if (is_extrema) 
                {
                    // 计算Harris角点响应值
                    float cornerness = computeCornerResponse(dog_space[s], x, y, s);
                    if (cornerness > 0.1) 
                    { // 角点响应值大于阈值,保留该点
                        cv::KeyPoint kp(x, y, 1.0f, -1, cornerness, s);
                        keypoints.push_back(kp);
                    }
                }
            }
        }
    }

    return keypoints;
}

int main(int argc, char** argv) 
{
    // 加载输入图像
    cv::Mat input_image = cv::imread("l.png", cv::IMREAD_GRAYSCALE);

    // 计算尺度空间和差分高斯图像
    std::vector<cv::Mat> dog_space = computeScaleSpace(input_image, 0.5, 3.0, 5);

    // 检测SIFT关键点
    std::vector<cv::KeyPoint> keypoints = detectKeypoints(dog_space);

    // 在原图上绘制关键点
    cv::Mat output_image;
    cv::drawKeypoints(input_image, keypoints, output_image, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

    // 显示结果
    // cv::imshow("SIFT Keypoints", output_image);
    cv::imwrite("sift_hessian_3.png", output_image);
    cv::waitKey(0);

    return 0;
}