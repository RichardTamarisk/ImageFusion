#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <map>

// 尺度空间定义
// L（x, y, σ）= G（x, y, σ）* I（x, y）
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

// 差分高斯方程计算
// D(x, y, σ) = L(x, y, kσ) - L(x, y, σ)
std::vector<cv::Mat> computeDifferenceOfGaussian(const std::vector<cv::Mat> &scale_space) 
{
    // 存储差分高斯图像的向量
    std::vector<cv::Mat> dog;

    // 遍历scale_space向量中相邻的图像
    for (size_t i = 0; i < scale_space.size() - 1; i++) 
    {
        // 存储当前尺度和上一个尺度之间的差分
        cv::Mat diff;

        // 计算当前尺度和上一个尺度之间的差分
        cv::subtract(scale_space[i + 1], scale_space[i], diff);
        
        // 将差分高斯图像添加到dog向量中
        dog.push_back(diff);
    }

    // 返回包含所有差分高斯图像的向量
    return dog;
}

// 在差分高斯图像序列中寻找局部极值点
std::vector<cv::Point3i> findExtrema(const std::vector<cv::Mat>& dog, double contrast_threshold) 
{

    // 存储所有找到的局部极值点的坐标
    std::vector<cv::Point3i> extrema;

    // 遍历差分高斯图像序列（除了第一张和最后一张）
    for (size_t i = 1; i < dog.size() - 1; i++) 
    {
        // 遍历当前差分高斯图像的所有像素点
        for (int y = 1; y < dog[i].rows - 1; y++) 
        {
            for (int x = 1; x < dog[i].cols - 1; x++) 
            {
                // 获取当前像素点的值
                // ∂D/∂X = 0
                double center = dog[i].at<double>(y, x);

                // 如果当前像素点的绝对值小于对比度阈值，则跳过
                // D(X) = D + 1/2(∂Dᵀ/∂X)X
                if (std::abs(center) < contrast_threshold) 
                {
                    continue;
                }

                // 假设当前像素点是局部极值点
                bool is_extrema = true;

                // 检查当前像素点及其26个邻域像素点
                for (int dy = -1; dy <= 1; dy++) 
                {
                    for (int dx = -1; dx <= 1; dx++) 
                    {
                        // 如果存在任何一个邻域像素点的绝对值大于等于当前像素点的绝对值，则不是局部极值点
                        //  ∂²D/∂X² < 0 or ∂²D/∂X² > 0
                        if (std::abs(dog[i + dy].at<double>(y + dy, x + dx)) >= std::abs(center)) 
                        {
                            is_extrema = false;
                            break;
                        }
                    }
                    if (!is_extrema) 
                    {
                        break;
                    }
                }

                // 如果是局部极值点，则将其坐标（x，y，x）添加到extrema向量中
                // X = -(∂²D⁻¹/∂X²)(∂D/∂X)
                if (is_extrema) 
                {
                    extrema.emplace_back(i, y, x);
                }
            }
        }
    }

    // 返回所有找到的局部极值点
    return extrema;
}

// 剔除不稳定的边缘响应点
std::vector<cv::Point3i> removeEdgeResponse(const std::vector<cv::Mat> &dog, const std::vector<cv::Point3i> &extrema, double r) 
{
    // 保存稳定的极值点
    std::vector<cv::Point3i> stable_extrema;

    // 遍历所有的极值点
    for (const auto& p : extrema) 
    {
        // 取出当前极值点的坐标
        int i = p.x, y = p.y, x = p.z;

        // 计算当前极值点处的 Hessian 矩阵分量
        double dxx = dog[i].at<double>(y - 1, x) - 2 * dog[i].at<double>(y, x) + dog[i].at<double>(y + 1, x);
        double dyy = dog[i].at<double>(y, x - 1) - 2 * dog[i].at<double>(y, x) + dog[i].at<double>(y, x + 1);
        double dxy = (dog[i].at<double>(y - 1, x - 1) - dog[i].at<double>(y - 1, x + 1) -
                      dog[i].at<double>(y + 1, x - 1) + dog[i].at<double>(y + 1, x + 1)) / 4;
        
        // 计算 Hessian 矩阵的对角元素之和和行列式
        double trace = dxx + dyy;
        double det = dxx * dyy - dxy * dxy;

        // 根据 Harris 角点检测条件，判断是否为稳定的特征点
        if (trace * trace / det < std::pow(r + 1, 2) / r) 
        {
            // 将当前极值点坐标添加到稳定的特征点集合中
            stable_extrema.push_back(p);
        }
    }

    // 稳定的特征点集合
    return stable_extrema;
}

// 特征点描述
// 计算特征点邻域内的梯度模值和方向
void computeGradientMagnitudeAndOrientation(const cv::Mat &image, 
                                           const cv::Point3i &keypoint, 
                                           std::vector<double> &magnitudes, 
                                           std::vector<double> &orientations)
{
    int i = keypoint.x, y = keypoint.y, x = keypoint.z;
    int radius = 8;  // 邻域窗口半径

    magnitudes.clear();
    orientations.clear();

    for (int dy = -radius; dy <= radius; dy++) 
    {
        for (int dx = -radius; dx <= radius; dx++) 
        {
            // 计算梯度模值和方向
            double dx_value = image.at<double>(y + dy, x + dx + 1) - image.at<double>(y + dy, x + dx - 1);
            double dy_value = image.at<double>(y + dy + 1, x + dx) - image.at<double>(y + dy - 1, x + dx);
            double magnitude = std::sqrt(dx_value * dx_value + dy_value * dy_value);
            double orientation = std::atan2(dy_value, dx_value);

            magnitudes.push_back(magnitude);
            orientations.push_back(orientation);
        }
    }
}

// 根据梯度方向分布构建特征点方向直方图
double assignKeyPointOrientation(const std::vector<double> &magnitudes,
                                 const std::vector<double> &orientations)
{
    // 初始化方向直方图,分成36个bin,每个bin代表10度的范围
    std::vector<double> histogram(36, 0.0);

    // 遍历邻域内的所有梯度,将其贡献加到对应的直方图bin中
    for (size_t i = 0; i < magnitudes.size(); i++) 
    {
        int bin = static_cast<int>(std::floor(orientations[i] * 180.0 / CV_PI / 10.0));
        histogram[bin] += magnitudes[i];
    }

    // 找到直方图中的峰值,作为特征点的主方向
    double max_value = 0.0;
    int max_bin = 0;
    for (int i = 0; i < 36; i++) 
    {
        if (histogram[i] > max_value) 
        {
            max_value = histogram[i];
            max_bin = i;
        }
    }

    // 返回特征点的主方向(弧度)
    return max_bin * CV_PI / 18.0;
}

// 分配特征点的方向
std::vector<cv::KeyPoint> computeSIFTKeyPoints(const cv::Mat &image,
                                               double contrast_threshold,
                                               double edge_threshold,
                                               double sigma_min,
                                               double sigma_max,
                                               int num_scales)
{
    // 计算尺度空间
    std::vector<cv::Mat> scale_space = computeScaleSpace(image, sigma_min, sigma_max, num_scales);

    // 计算差分高斯图像序列
    std::vector<cv::Mat> dog = computeDifferenceOfGaussian(scale_space);

    // 寻找局部极值点
    std::vector<cv::Point3i> extrema = findExtrema(dog, contrast_threshold);

    // 剔除不稳定的边缘响应点
    std::vector<cv::Point3i> stable_extrema = removeEdgeResponse(dog, extrema, edge_threshold);

    // 为每个稳定的极值点分配方向
    std::vector<cv::KeyPoint> keypoints;
    for (const auto& p : stable_extrema) {
        std::vector<double> magnitudes, orientations;
        computeGradientMagnitudeAndOrientation(dog[p.x], p, magnitudes, orientations);
        double orientation = assignKeyPointOrientation(magnitudes, orientations);

        cv::KeyPoint kp(p.z, p.y, 1.0, orientation, 0.0, 0, -1);
        keypoints.push_back(kp);
    }

    return keypoints;
}

int main(int argc, char **argv)
{
    // 读取输入图像
    cv::Mat image = cv::imread("/home/jack/Documents/code/ImageFusion/doc/image/l.png", cv::IMREAD_COLOR);
    if (image.empty())
    {
        std::cerr << "Failed to load image." << std::endl;
        return 1;
    }

    // 计算尺度空间
    double sigma_min = 1.6;
    double sigma_max = 640;
    int num_scales = 500;
    std::vector<cv::Mat> scale_space = computeScaleSpace(image, sigma_min, sigma_max, num_scales);

    // 计算差分高斯图像序列
    std::vector<cv::Mat> dog = computeDifferenceOfGaussian(scale_space);

    // 寻找差分高斯图像序列中的局部极值点
    double contrast_threshold = 0.5;
    std::vector<cv::Point3i> extrema = findExtrema(dog, contrast_threshold);

    // 剔除不稳定的边缘响应点
    double edge_threshold = 0.1;
    std::vector<cv::Point3i> stable_extrema = removeEdgeResponse(dog, extrema, edge_threshold);

    // 计算特征点描述子
    std::vector<cv::Mat> descriptors;
    for (const auto& p : stable_extrema)
    {
        std::vector<double> magnitude, orientation;
        computeGradientMagnitudeAndOrientation(image, p, magnitude, orientation);
        cv::Mat descriptor;
        descriptors.push_back(descriptor);
    }

    // 使用 'stable_extrema' 和 'descriptors' 向量进行后续的特征匹配和应用

    // 在原图上描绘特征点
    cv::Mat result = image.clone();
    for(const auto& p : stable_extrema)
    {
        cv::circle(result, cv::Point(p.x, p.y), 3, cv::Scalar(0, 0, 255), 1);
    }

    imwrite("sift_hessian_1.png",result);

    return 0;
}