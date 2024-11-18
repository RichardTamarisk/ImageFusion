#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/flann.hpp>
#include <opencv2/opencv.hpp>
#include "correct.h"


int main(int argc, char** argv)
{
    // 进行图像纠正
    cv::Mat corrected_img1, corrected_img2;
    correctedImage("new_l_2.jpg", corrected_img1);
    correctedImage("new_r_2.jpg", corrected_img2);

    cv::imwrite("corrected_l.jpg", corrected_img1);
    cv::imwrite("corrected_r.jpg", corrected_img2);

    // // 不进行图像矫正
    // cv::Mat corrected_img1 = cv::imread("new_l_2.jpg");
    // cv::Mat corrected_img2 = cv::imread("new_r_2.jpg");

    if (corrected_img1.empty() || corrected_img2.empty())
    {
        std::cerr << "Error: Failed to load corrected images." << std::endl;
        return 1;
    }

    // 创建SIFT特征检测器
    cv::Ptr<cv::SIFT> detector = SIFT::create();

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

        // 箭头的终点
        cv::Point2f end_pt = pt + cv::Point2f(10 * cos(angle * CV_PI / 180), 10 * sin(angle * CV_PI / 180));
        cv::arrowedLine(img_arrows1, pt, end_pt, cv::Scalar(0, 255, 0), 1);
    }

    for (const auto& keypoint : keypoints2)
    {
        cv::Point2f pt = keypoint.pt;
        float angle = keypoint.angle;  

        // 箭头的终点
        cv::Point2f end_pt = pt + cv::Point2f(10 * cos(angle * CV_PI / 180), 10 * sin(angle * CV_PI / 180));
        cv::arrowedLine(img_arrows2, pt, end_pt, cv::Scalar(0, 255, 0), 1);
    }

    // 保存绘制的箭头图像
    cv::imwrite("arrows_l.jpg", img_arrows1);
    cv::imwrite("arrows_r.jpg", img_arrows2);

    // 创建基于FLANN的描述子匹配器
    Ptr<cv::DescriptorMatcher> matcher = DescriptorMatcher::create(DescriptorMatcher::FLANNBASED);

    // 使用FLANN进行描述子匹配
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

    // 提取匹配的关键点
    std::vector<cv::Point2f> points1, points2;
    for (const auto& match : good_matches)
    {
        points1.push_back(keypoints1[match.queryIdx].pt);
        points2.push_back(keypoints2[match.trainIdx].pt);
    }

    // 使用RANSAC算法计算单应性变换矩阵
    cv::Mat homography;
    homography = findHomography(points2, points1, cv::RANSAC);

    // 透视变换
    cv::Mat dst;
    warpPerspective(corrected_img2, dst, homography, Size(corrected_img1.cols + corrected_img2.cols, corrected_img1.rows + corrected_img2.rows));

    // 将图像A拼接到透视变换结果上
    Rect roi_rect = Rect(0, 0, corrected_img1.cols, corrected_img1.rows);
    corrected_img1.copyTo(dst(roi_rect));

    // 输出处理时间
    auto t1 = std::chrono::high_resolution_clock::now();
    // 透视变换和拼接
    auto t2 = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double, std::milli> fp_ms = t2 - t1;
    std::cout << "Processing time: " << fp_ms.count() << " ms" << std::endl;

    // 保存拼接后的图像
    imwrite("sift_corrected.jpg", dst);

    // 显示结果
    // imshow("Panorama", dst);
    waitKey(0);

    return 0;
}
