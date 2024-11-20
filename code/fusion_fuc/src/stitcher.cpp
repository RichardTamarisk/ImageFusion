#include "stitcher.h"
#include <opencv2/highgui.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <chrono>
#include <opencv2/calib3d.hpp>
#include <opencv2/opencv.hpp>

void correctImage(const char* filename, cv::Mat& output) 
{
    clock_t start_time = clock();
    cv::Mat img = cv::imread(filename);  // 添加 cv::
    cv::Mat drcimg(img.rows, img.cols, CV_8UC3);

    cv::Point lenscenter(img.cols / 2, img.rows / 2); // 添加 cv::
    cv::Point2f src_a, src_b, src_c, src_d; // 添加 cv::
    double r; // 矫正前像素点跟镜头中心的距离
    double s; // 矫正后像素点跟镜头中心的距离
    cv::Point2f mCorrectPoint; // 添加 cv::
    double distance_to_a_x, distance_to_a_y; // 求得中心点和边界的距离

    for (int row = 0; row < img.rows; row++) // 操作数据区,要注意OpenCV的RGB的存储顺序为BGR
    {
        for (int cols = 0; cols < img.cols; cols++) // 示例为亮度调节
        {
            // 计算当前像素点到图像中心点的距离
            r = sqrt((row - lenscenter.y) * (row - lenscenter.y) + (cols - lenscenter.x) * (cols - lenscenter.x)) * 0.56;
            // 根据距离 r 计算比例因子 s
            s = 0.9998 - 4.2932 * pow(10, -4) * r + 3.4327 * pow(10, -6) * pow(r, 2) - 2.8526 * pow(10, -9) * pow(r, 3) + 9.8223 * pow(10, -13) * pow(r, 4); // 比例
            // 计算修正后的像素点位置
            mCorrectPoint = cv::Point2f((cols - lenscenter.x) / s * 1.35 + lenscenter.x, (row - lenscenter.y) / s * 1.35 + lenscenter.y); // 添加 cv::

            // 越界判断
            if (mCorrectPoint.y < 0 || mCorrectPoint.y >= img.rows - 1)
            {
                continue;
            }
            if (mCorrectPoint.x < 0 || mCorrectPoint.x >= img.cols - 1)
            {
                continue;
            }

            // 计算临近点的坐标
            src_a = cv::Point2f((int)mCorrectPoint.x, (int)mCorrectPoint.y); // 添加 cv::
            src_b = cv::Point2f(src_a.x + 1, src_a.y); // 添加 cv::
            src_c = cv::Point2f(src_a.x, src_a.y + 1); // 添加 cv::
            src_d = cv::Point2f(src_a.x + 1, src_a.y + 1); // 添加 cv::

            // 计算当前像素点的新像素值
            distance_to_a_x = mCorrectPoint.x - src_a.x;
            distance_to_a_y = mCorrectPoint.y - src_a.y;

            // 使用双线性插值计算新的像素值
            drcimg.at<cv::Vec3b>(row, cols)[0] =
                img.at<cv::Vec3b>(src_a.y, src_a.x)[0] * (1 - distance_to_a_x) * (1 - distance_to_a_y) +
                img.at<cv::Vec3b>(src_b.y, src_b.x)[0] * distance_to_a_x * (1 - distance_to_a_y) +
                img.at<cv::Vec3b>(src_c.y, src_c.x)[0] * distance_to_a_y * (1 - distance_to_a_x) +
                img.at<cv::Vec3b>(src_d.y, src_d.x)[0] * distance_to_a_y * distance_to_a_x;

            drcimg.at<cv::Vec3b>(row, cols)[1] =
                img.at<cv::Vec3b>(src_a.y, src_a.x)[1] * (1 - distance_to_a_x) * (1 - distance_to_a_y) +
                img.at<cv::Vec3b>(src_b.y, src_b.x)[1] * distance_to_a_x * (1 - distance_to_a_y) +
                img.at<cv::Vec3b>(src_c.y, src_c.x)[1] * distance_to_a_y * (1 - distance_to_a_x) +
                img.at<cv::Vec3b>(src_d.y, src_d.x)[1] * distance_to_a_y * distance_to_a_x;

            drcimg.at<cv::Vec3b>(row, cols)[2] =
                img.at<cv::Vec3b>(src_a.y, src_a.x)[2] * (1 - distance_to_a_x) * (1 - distance_to_a_y) +
                img.at<cv::Vec3b>(src_b.y, src_b.x)[2] * distance_to_a_x * (1 - distance_to_a_y) +
                img.at<cv::Vec3b>(src_c.y, src_c.x)[2] * distance_to_a_y * (1 - distance_to_a_x) +
                img.at<cv::Vec3b>(src_d.y, src_d.x)[2] * distance_to_a_y * distance_to_a_x;
        }
    }

    // 将drcimg转换为cv::Mat对象
    drcimg.convertTo(output, CV_8UC3);

    // 裁剪
    int topCropHeight = drcimg.rows * 0.126; // 上方裁剪
    int bottomCropHeight = drcimg.rows * 0.126; // 下方裁剪
    int leftCropWidth = drcimg.cols * 0.083; // 左侧裁剪
    int rightCropWidth = drcimg.cols * 0.085; // 右侧裁剪

    cv::Rect roi(leftCropWidth, topCropHeight, drcimg.cols - leftCropWidth - rightCropWidth, drcimg.rows - topCropHeight - bottomCropHeight);
    cv::Mat croppedImg = drcimg(roi);
    output = croppedImg;
}

bool stitchImages(const char* img1_path, const char* img2_path, const char* output_path) 
{
    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat corrected_img1, corrected_img2;
    correctImage(img1_path, corrected_img1);
    correctImage(img2_path, corrected_img2);

    if (corrected_img1.empty() || corrected_img2.empty()) {
        std::cerr << "Error: Failed to load corrected images." << std::endl;
        return false;
    }

    // 创建 SIFT 特征检测器
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create(1000);
    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    cv::Mat descriptors1, descriptors2;
    detector->detectAndCompute(corrected_img1, cv::Mat(), keypoints1, descriptors1);
    detector->detectAndCompute(corrected_img2, cv::Mat(), keypoints2, descriptors2);

    // 创建暴力匹配器并进行描述子匹配
    cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_L2);
    std::vector<std::vector<cv::DMatch>> matches;
    matcher->knnMatch(descriptors1, descriptors2, matches, 2);

    // 筛选出良好的匹配点对
    std::vector<cv::DMatch> good_matches;
    for (const auto& match : matches) {
        if (match[0].distance < 0.7 * match[1].distance) {
            good_matches.push_back(match[0]);
        }
    }

    // 提取匹配的关键点
    std::vector<cv::Point2f> points1, points2;
    for (const auto& match : good_matches) {
        points1.push_back(keypoints1[match.queryIdx].pt);
        points2.push_back(keypoints2[match.trainIdx].pt);
    }

    // 使用 RANSAC 算法计算透视变换
    cv::Mat homography = cv::findHomography(points2, points1, cv::RANSAC, 5.0);
    if (homography.empty()) {
        std::cerr << "Homography matrix is empty." << std::endl;
        return false;
    }

    // 拼接图像
    int dst_width = corrected_img1.cols + corrected_img2.cols;
    int dst_height = std::max(corrected_img1.rows, corrected_img2.rows);
    cv::Mat dst = cv::Mat::zeros(dst_height, dst_width, corrected_img1.type());

    // 将图像 1 复制到目标图像
    corrected_img1.copyTo(dst(cv::Rect(0, 0, corrected_img1.cols, corrected_img1.rows)));

    // 将图像 2 透视变换并复制到目标图像
    cv::Mat transformed_img2;
    cv::warpPerspective(corrected_img2, transformed_img2, homography, dst.size());

    // 自动识别重叠区域
    std::vector<cv::Point2f> corners = 
    {
        cv::Point2f(0, 0),
        cv::Point2f(corrected_img1.cols, 0),
        cv::Point2f(corrected_img1.cols, corrected_img1.rows),
        cv::Point2f(0, corrected_img1.rows)
    };

    std::vector<cv::Point2f> transformed_corners;
    cv::perspectiveTransform(corners, transformed_corners, homography);
    cv::Rect transformed_rect = cv::boundingRect(transformed_corners);
    cv::Rect overlap_rect = cv::Rect(0, 0, corrected_img1.cols, corrected_img1.rows) & transformed_rect;

    // 提取重叠区域的起始横坐标和宽度
    int overlap_start_x = overlap_rect.x;
    int overlap_width = overlap_rect.width;

    // 打印重叠区域信息
    std::cout << "Overlap Start X: " << overlap_start_x << std::endl;
    std::cout << "Overlap Width: " << overlap_width << std::endl;

    // 进行渐入渐出法处理重叠区域
    for (int y = overlap_rect.y; y < overlap_rect.y + overlap_rect.height; y++) 
    {
        for (int x = overlap_rect.x; x < overlap_rect.x + overlap_rect.width; x++) 
        {
            // 计算权重
            float d1 = static_cast<float>(overlap_rect.x + overlap_rect.width - x) / overlap_width; 
            float d2 = static_cast<float>(x - overlap_rect.x) / overlap_width;

            // 确保权重在 [0, 1] 之间
            d1 = std::clamp(d1, 0.0f, 1.0f);
            d2 = std::clamp(d2, 0.0f, 1.0f);

            // 获取左图和右图的像素
            cv::Vec3b pixel1 = corrected_img1.at<cv::Vec3b>(y, x);
            cv::Vec3b pixel2 = transformed_img2.at<cv::Vec3b>(y, x);

            // 进行加权平均（逐通道处理）
            cv::Vec3b blended_pixel;
            blended_pixel[0] = cv::saturate_cast<uchar>(pixel1[0] * d1 + pixel2[0] * d2);
            blended_pixel[1] = cv::saturate_cast<uchar>(pixel1[1] * d1 + pixel2[1] * d2);
            blended_pixel[2] = cv::saturate_cast<uchar>(pixel1[2] * d1 + pixel2[2] * d2);

            // 更新拼接后图像的重叠区域像素
            dst.at<cv::Vec3b>(y, x) = blended_pixel;
        }
    }

    // 处理右图的非重叠部分
    cv::Rect right_non_overlap_rect(overlap_rect.x + overlap_rect.width, 0, transformed_img2.cols - overlap_rect.x - overlap_rect.width, transformed_img2.rows);
    if (right_non_overlap_rect.width > 0 && right_non_overlap_rect.x < dst.cols) 
    {
        transformed_img2(right_non_overlap_rect).copyTo(dst(right_non_overlap_rect));
    }
    // 保存拼接后的图像
    cv::imwrite(output_path, dst);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Time taken: " << duration.count() << " seconds." << std::endl;

    return true;
}