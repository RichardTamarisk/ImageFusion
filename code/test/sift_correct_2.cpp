#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include "correct.h"

int main(int argc, char** argv)
{
    // // 进行图像纠正
    // cv::Mat corrected_img1, corrected_img2;
    // correctedImage("left_1.jpg", corrected_img1);
    // correctedImage("right_1.jpg", corrected_img2);

    // 不进行图像矫正
    cv::Mat corrected_img1 = cv::imread("left_1.jpg");
    cv::Mat corrected_img2 = cv::imread("right_1.jpg");

    if (corrected_img1.empty() || corrected_img2.empty())
    {
        std::cerr << "Error: Failed to load corrected images." << std::endl;
        return 1;
    }

    // 创建SIFT特征检测器
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create(100000);

    // 检测SIFT关键点和描述子
    std::vector<cv::KeyPoint> keypoints1, keypoints2;
    cv::Mat descriptors1, descriptors2;
    detector->detectAndCompute(corrected_img1, cv::Mat(), keypoints1, descriptors1);
    detector->detectAndCompute(corrected_img2, cv::Mat(), keypoints2, descriptors2);

    // 绘制特征点
    cv::Mat img_arrows1 = corrected_img1.clone();
    cv::Mat img_arrows2 = corrected_img2.clone();

    for (const auto& keypoint : keypoints1)
    {
        cv::Point2f pt = keypoint.pt;
        float angle = keypoint.angle;
        cv::Point2f end_pt = pt + cv::Point2f(10 * cos(angle * CV_PI / 180), 10 * sin(angle * CV_PI / 180));
        cv::arrowedLine(img_arrows1, pt, end_pt, cv::Scalar(0, 255, 0), 1);
    }

    for (const auto& keypoint : keypoints2)
    {
        cv::Point2f pt = keypoint.pt;
        float angle = keypoint.angle;
        cv::Point2f end_pt = pt + cv::Point2f(10 * cos(angle * CV_PI / 180), 10 * sin(angle * CV_PI / 180));
        cv::arrowedLine(img_arrows2, pt, end_pt, cv::Scalar(0, 255, 0), 1);
    }

    // 保存特征点图像
    cv::imwrite("keypoints1.jpg", img_arrows1);
    cv::imwrite("keypoints2.jpg", img_arrows2);

    // 创建暴力匹配器并进行描述子匹配
    cv::Ptr<cv::BFMatcher> matcher = cv::BFMatcher::create(cv::NORM_L2);
    std::vector<std::vector<cv::DMatch>> matches;
    matcher->knnMatch(descriptors1, descriptors2, matches, 2);

    // 筛选出良好的匹配点对
    std::vector<cv::DMatch> good_matches;  
    for (size_t i = 0; i < matches.size(); i++)
    {
        if (matches[i][0].distance < 0.7 * matches[i][1].distance)
        {
            good_matches.push_back(matches[i][0]);
        }
    }

    // 输出成功匹配的特征点数量
    std::cout << "Number of good matches: " << good_matches.size() << std::endl;

    // 检查匹配点数量
    if (good_matches.size() < 4) {
        std::cerr << "Not enough good matches to compute homography." << std::endl;
        return -1;
    }

    // 创建一个新的图像用于显示匹配结果
    cv::Mat match_image(corrected_img1.rows, corrected_img1.cols + corrected_img2.cols, corrected_img1.type());
    corrected_img1.copyTo(match_image(cv::Rect(0, 0, corrected_img1.cols, corrected_img1.rows)));
    corrected_img2.copyTo(match_image(cv::Rect(corrected_img1.cols, 0, corrected_img2.cols, corrected_img2.rows)));

    // 提取匹配的关键点
    std::vector<cv::Point2f> points1, points2;
    for (const auto& match : good_matches)
    {
        points1.push_back(keypoints1[match.queryIdx].pt);
        points2.push_back(keypoints2[match.trainIdx].pt);
    }

    // 在匹配的特征点之间绘制线
    for (const auto& match : good_matches)
    {
        cv::Point2f pt1 = keypoints1[match.queryIdx].pt;
        cv::Point2f pt2 = keypoints2[match.trainIdx].pt + cv::Point2f(corrected_img1.cols, 0); // 调整为合并图像的位置
        cv::line(match_image, pt1, pt2, cv::Scalar(255, 0, 0), 1); // 红色线
    }

    // 保存匹配结果图像
    cv::imwrite("matches.jpg", match_image);

    // 使用RANSAC算法计算透视变换
    cv::Mat homography = cv::findHomography(points2, points1, cv::RANSAC, 5.0);

    // 检查单应性矩阵
    if (homography.empty()) {
        std::cerr << "Homography matrix is empty." << std::endl;
        return -1;
    }

    // 保存拼接后的图像
    cv::Mat dst;
    auto t1 = std::chrono::high_resolution_clock::now();
    cv::warpPerspective(corrected_img2, dst, homography, cv::Size(corrected_img1.cols + corrected_img2.cols, corrected_img1.rows));

    // 将图像A拼接到透视变换结果上
    cv::Rect roi_rect(0, 0, corrected_img1.cols, corrected_img1.rows);
    if (roi_rect.x >= 0 && roi_rect.y >= 0 && roi_rect.width <= dst.cols && roi_rect.height <= dst.rows) {
        corrected_img1.copyTo(dst(roi_rect));
    } else {
        std::cerr << "ROI is out of bounds." << std::endl;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> fp_ms = t2 - t1;
    std::cout << "Processing time: " << fp_ms.count() << " ms" << std::endl;

    // 保存拼接后的图像
    cv::imwrite("result.jpg", dst);

    // 显示结果
    // cv::imshow("Matches", match_image);
    cv::waitKey(0);

    return 0;
}