#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/flann.hpp>
#include <opencv2/opencv.hpp>
#include <chrono>
#include "correct_frame.h" 

int main(int argc, char** argv) 
{
    auto start_time = std::chrono::high_resolution_clock::now(); // 记录总开始时间

    if (argc < 3) 
    {
        std::cerr << "Usage: " << argv[0] << " <video1> <video2>" << std::endl;
        return 1;
    }

    cv::VideoCapture cap1(argv[1]);
    cv::VideoCapture cap2(argv[2]);

    if (!cap1.isOpened() || !cap2.isOpened()) 
    {
        std::cerr << "Error: Could not open one of the video files." << std::endl;
        return 1;
    }

    cv::Mat frame1, frame2;
    cv::Mat corrected_frame1, corrected_frame2; // 纠正后的帧
    std::vector<cv::Mat> stitched_frames;

    // 创建SIFT特征检测器
    cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

    int frame_count = 0;
    cv::Mat homography;

    while (true) 
    {
        // 每46帧进行特征提取和匹配
        if (frame_count % 1000 == 0) 
        {
            // 读取第一帧并进行几何畸变校正
            if (!cap1.read(frame1)) 
            {
                std::cerr << "Error: Could not read frame from video 1." << std::endl;
                return 1;
            }
            correctedImage(frame1, corrected_frame1);
            imwrite("corrected_frame1.jpg", corrected_frame1); // 保存校正后的帧

            // 读取第二个视频的第一帧并进行几何畸变校正
            if (!cap2.read(frame2)) 
            {
                std::cerr << "Error: Could not read frame from video 2." << std::endl;
                break;
            }
            correctedImage(frame2, corrected_frame2); 
            imwrite("corrected_frame2.jpg", corrected_frame2); // 保存校正后的帧

            // 获取帧的宽度和高度
            int width1 = corrected_frame1.cols;
            int width2 = corrected_frame2.cols;
            int height1 = corrected_frame1.rows;
            int height2 = corrected_frame2.rows;

            // 选取第一个视频的最右边1/4
            cv::Rect roi1(width1 * 3 / 4, 0, width1 / 4, height1); // 最右边1/4区域
            cv::Mat frame1_roi = corrected_frame1(roi1);

            // 选取第二个视频的最左边1/4
            cv::Rect roi2(0, 0, width2 / 4, height2); // 最左边1/4区域
            cv::Mat frame2_roi = corrected_frame2(roi2);

            std::vector<cv::KeyPoint> keypoints1, keypoints2;
            cv::Mat descriptors1, descriptors2;

            // 提取特征点和描述子
            detector->detectAndCompute(frame1_roi, cv::Mat(), keypoints1, descriptors1);
            detector->detectAndCompute(frame2_roi, cv::Mat(), keypoints2, descriptors2);

            // 创建基于FLANN的描述子匹配器
            cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);

            // 进行匹配
            std::vector<cv::DMatch> matches;
            matcher->match(descriptors1, descriptors2, matches);

            // 提取匹配的关键点
            std::vector<cv::Point2f> points1, points2;
            for (const auto& match : matches) 
            {
                points1.push_back(keypoints1[match.queryIdx].pt + cv::Point2f(roi1.x, roi1.y)); // 添加ROI偏移
                points2.push_back(keypoints2[match.trainIdx].pt + cv::Point2f(roi2.x, roi2.y)); // 添加ROI偏移
            }

            // 使用RANSAC算法计算单应性变换矩阵
            homography = findHomography(points2, points1, cv::RANSAC);
            if (homography.empty()) 
            {
                std::cerr << "Failed to compute homography." << std::endl;
                return 1;
            }

            // 将第二帧的透视变换
            cv::Mat dst;
            // cv::warpPerspective(corrected_frame2, dst, homography, cv::Size(corrected_frame1.cols + corrected_frame2.cols, corrected_frame1.rows));
            cv::warpPerspective(corrected_frame2, dst, homography, cv::Size(frame1.cols + (frame2.cols * 0.75), frame1.rows));

            // 将第一帧复制到透视变换结果上
            cv::Rect roi_rect(0, 0, corrected_frame1.cols, corrected_frame1.rows);
            corrected_frame1.copyTo(dst(roi_rect));

            // 保存拼接后的帧
            stitched_frames.push_back(dst.clone());
        } 
        else 
        {
            // 读取第二个视频的后续帧并进行几何畸变校正
            if (!cap2.read(frame2)) 
            {
                break; // 结束条件：视频读取完毕
            }
            correctedImage(frame2, corrected_frame2); // 进行几何畸变校正

            // 使用之前计算的homography进行透视变换
            cv::Mat dst;
            // cv::warpPerspective(corrected_frame2, dst, homography, cv::Size(corrected_frame1.cols + corrected_frame2.cols, corrected_frame1.rows));
            cv::warpPerspective(corrected_frame2, dst, homography, cv::Size(frame1.cols + (frame2.cols * 0.5), frame1.rows));

            // 将第一帧复制到透视变换结果上
            cv::Rect roi_rect(0, 0, corrected_frame1.cols, corrected_frame1.rows);
            corrected_frame1.copyTo(dst(roi_rect));

            // 保存拼接后的帧
            stitched_frames.push_back(dst.clone());
        }

        // 读取第一个视频的后续帧并进行几何畸变校正
        if (!cap1.read(frame1)) 
        {
            break; // 结束条件：视频读取完毕
        }
        correctedImage(frame1, corrected_frame1); // 进行几何畸变校正

        frame_count++;

        // 输出处理进度
        auto frame_end_time = std::chrono::high_resolution_clock::now(); // 记录每帧结束时间
        std::chrono::duration<double> frame_elapsed = frame_end_time - start_time;
        std::cout << "Processed frame " << frame_count << " in " << frame_elapsed.count() << " seconds." << std::endl;

        // if(frame_count == 2)
        //     break;
    }

    // 输出处理后的结果
    if (!stitched_frames.empty()) 
    {
        cv::VideoWriter outputVideo("stitched_output.mp4",
                                     cv::VideoWriter::fourcc('H', '2', '6', '4'),
                                     46,
                                     stitched_frames[0].size());
        for (size_t j = 0; j < stitched_frames.size(); j++) 
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