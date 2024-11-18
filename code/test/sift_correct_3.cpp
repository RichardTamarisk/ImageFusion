#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include "correct.h"

int main(int argc, char** argv) {
    // 进行图像纠正
    cv::Mat corrected_img1, corrected_img2;
    correctedImage("left_1.jpg", corrected_img1);
    correctedImage("right_1.jpg", corrected_img2);

    if (corrected_img1.empty() || corrected_img2.empty()) {
        std::cerr << "Error: Failed to load corrected images." << std::endl;
        return 1;
    }

    // 创建 SIFT 特征检测器
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create();
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
    for (size_t i = 0; i < matches.size(); i++) {
        if (matches[i][0].distance < 0.7 * matches[i][1].distance) {
            good_matches.push_back(matches[i][0]);
        }
    }

    // 检查匹配点数量
    if (good_matches.size() < 4) {
        std::cerr << "Not enough good matches to compute homography." << std::endl;
        return -1;
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
        return -1;
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

    // 确定重叠区域
    int overlap_start_x = 785; // 根据实际重叠计算
    int overlap_width = 346; // 根据实际重叠计算
    cv::Rect overlap_rect(overlap_start_x, 0, overlap_width, corrected_img1.rows);

    // 进行渐入渐出法处理重叠区域
    for (int y = overlap_rect.y; y < overlap_rect.y + overlap_rect.height; y++) {
        for (int x = overlap_rect.x; x < overlap_rect.x + overlap_rect.width; x++) {
            // 计算权重
            float d1 = static_cast<float>(overlap_rect.x + overlap_rect.width - x) / overlap_width; // f1 权重
            float d2 = static_cast<float>(x - overlap_rect.x) / overlap_width; // f2 权重

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
    if (right_non_overlap_rect.width > 0 && right_non_overlap_rect.x < dst.cols) {
        transformed_img2(right_non_overlap_rect).copyTo(dst(right_non_overlap_rect));
    }

    // 保存拼接后的图像
    cv::imwrite("result.jpg", dst);
    std::cout << "Image stitching completed." << std::endl;

    return 0;
}