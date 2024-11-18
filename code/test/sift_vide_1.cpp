// 对每一帧进行特征点配对

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/flann.hpp>
#include <opencv2/opencv.hpp>
#include <chrono>

int main(int argc, char** argv)
{
    auto start_time = std::chrono::high_resolution_clock::now(); // 记录总开始时间

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << "./DisplayImage <video1> <video2>" << std::endl;
        return 1;
    }

    // cv::VideoCapture cap1("left.h264");
    // cv::VideoCapture cap2("right.h264");
    cv::VideoCapture cap1(argv[1]);
    cv::VideoCapture cap2(argv[2]);

    if (!cap1.isOpened() || !cap2.isOpened()) {
        std::cerr << "Error: Could not open one of the video files." << std::endl;
        return 1;
    }

    cv::Mat frame1, frame2;
    std::vector<cv::Mat> stitched_frames;

    // 创建SIFT特征检测器
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

    int i = 0;
    while (true) 
    {
        // 读取视频帧
        if (!cap1.read(frame1) || !cap2.read(frame2)) 
        {
            break; // 结束条件：任一视频读取完毕
        }

        auto frame_start_time = std::chrono::high_resolution_clock::now(); // 记录每帧开始时间

        // 检测SIFT关键点和描述子
        std::vector<cv::KeyPoint> keypoints1, keypoints2;
        cv::Mat descriptors1, descriptors2;
        detector->detectAndCompute(frame1, cv::Mat(), keypoints1, descriptors1);
        detector->detectAndCompute(frame2, cv::Mat(), keypoints2, descriptors2);

        // 创建基于FLANN的描述子匹配器
        cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
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
        cv::Mat homography = findHomography(points2, points1, cv::RANSAC);

        // 透视变换
        cv::Mat dst;
        cv::warpPerspective(frame2, dst, homography, cv::Size(frame1.cols + frame2.cols, frame1.rows));

        // 将第一帧复制到透视变换结果上
        cv::Rect roi_rect(0, 0, frame1.cols, frame1.rows);
        frame1.copyTo(dst(roi_rect));

        // 保存拼接后的帧
        stitched_frames.push_back(dst.clone());

        auto frame_end_time = std::chrono::high_resolution_clock::now(); // 记录每帧结束时间
        std::chrono::duration<double> frame_elapsed = frame_end_time - frame_start_time;
        std::cout << "Processed frame " << i++ << " in " << frame_elapsed.count() << " seconds." << std::endl;
    }

    // 输出处理后的结果
    if (!stitched_frames.empty()) 
    {
        cv::VideoWriter outputVideo("stitched_output.mp4", cv::VideoWriter::fourcc('H','2','6','4'), 25, stitched_frames[0].size());
        // for (const auto& frame : stitched_frames) 
        // {
        //     outputVideo.write(frame);
        // }
        for(size_t j = 0; j < stitched_frames.size(); j++)
        {
            outputVideo.write(stitched_frames[j]);
        }
    }

    cap1.release();
    cap2.release();

    auto end_time = std::chrono::high_resolution_clock::now(); // 记录总结束时间
    std::chrono::duration<double> total_elapsed = end_time - start_time;
    std::cout << "Video stitching completed in " << total_elapsed.count() << " seconds." << std::endl;

    return 0;
}