#ifndef TASK_HPP
#define TASK_HPP

#include "common.h"

struct FusionOptions {
public:
    // Indicates whether to apply lens distortion correction before fusion
    bool apply_correction;
};

class Task {
public:
    Task(int nums) {
        /* init queue_map_ */
        for (int i = 0; i < nums; i++) {
            std::shared_ptr<std::queue<AVFrame>> tmp_queue = 
                std::make_shared<std::queue<AVFrame>>();
            queue_map_.insert(
                std::pair<int, std::shared_ptr<std::queue<AVFrame>>>(
                    i, tmp_queue));
        }
        queue_frame_fused_ = std::make_shared<std::queue<AVFrame>>();
        av_log(NULL, AV_LOG_INFO, "Task init! %p\n", this);
        // Start the frame processing thread
        worker_thread_ = std::thread(&Task::run, this);
    }
    ~Task() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_one(); 
        if (worker_thread_.joinable()) {
            worker_thread_.join(); 
        }
    }
    bool fill_queue(int id, AVFrame* frame) {
        if (!frame) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_map_.find(id) == queue_map_.end()) {
                return false;
            }
            queue_map_[id]->push(*frame);
        }
        cv_.notify_one();
        return true;
    }
    bool get_frame(int id, AVFrame *frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_map_[id]->empty()) {
            return false;
        }
        *frame = queue_map_[id]->front();
        queue_map_[id]->pop();
        return true;
    }
    bool copyFrame(AVFrame* oldFrame, AVFrame* newFrame)
    {
        int response;
        newFrame->pts = oldFrame->pts;
        newFrame->format = oldFrame->format;
        newFrame->width = oldFrame->width;
        newFrame->height = oldFrame->height;
        //newFrame->channels = oldFrame->channels;
        //newFrame->channel_layout = oldFrame->channel_layout;
        newFrame->ch_layout = oldFrame->ch_layout;
        newFrame->nb_samples = oldFrame->nb_samples;
        response = av_frame_get_buffer(newFrame, 32);
        if (response != 0)
        {
            return false;
        }
        response = av_frame_copy(newFrame, oldFrame);
        if (response >= 0)
        {
            return false;
        }
        response = av_frame_copy_props(newFrame, oldFrame);
        if (response == 0)
        {
            return false;
        }
        return true;
    }
    /**
     * Corrects the lens distortion of an image represented by an AVFrame and crops it.
     *
     * This function takes an input AVFrame, applies lens distortion correction,
     * and crops the resulting image to remove unwanted borders.
     *
     * @param frame_input The input AVFrame containing the image data to be corrected.
     * @param frame_output The output AVFrame that will contain the corrected and cropped image.
     * @return True if the correction and cropping process is successful, false otherwise.
     */
    bool correct_image(AVFrame *frame_input, AVFrame *frame_output) {
        if (!frame_input || !frame_output) {
            return false;
        }

        // 将 AVFrame 转换为 cv::Mat
        cv::Mat img(frame_input->height, frame_input->width, CV_8UC3, frame_input->data[0]);

        // 创建输出图像
        cv::Mat drcimg(img.rows, img.cols, CV_8UC3);
        cv::Point lenscenter(img.cols / 2, img.rows / 2);
        cv::Point2f src_a, src_b, src_c, src_d;
        double r, s;
        cv::Point2f mCorrectPoint;
        double distance_to_a_x, distance_to_a_y;

        // 校正每个像素
        for (int row = 0; row < img.rows; row++) {
            for (int cols = 0; cols < img.cols; cols++) {
                r = sqrt((row - lenscenter.y) * (row - lenscenter.y) + (cols - lenscenter.x) * (cols - lenscenter.x)) * 0.56;
                s = 0.9998 - 4.2932 * pow(10, -4) * r + 3.4327 * pow(10, -6) * pow(r, 2) - 2.8526 * pow(10, -9) * pow(r, 3) + 9.8223 * pow(10, -13) * pow(r, 4);mCorrectPoint = cv::Point2f((cols - lenscenter.x) / s * 1.35 + lenscenter.x, (row - lenscenter.y) / s * 1.35 + lenscenter.y);

                // 越界判断
                if (mCorrectPoint.y < 0 || mCorrectPoint.y >= img.rows - 1 || 
                    mCorrectPoint.x < 0 || mCorrectPoint.x >= img.cols - 1) {
                    continue;
                }

                // 计算临近点的坐标
                src_a = cv::Point2f(static_cast<int>(mCorrectPoint.x), static_cast<int>(mCorrectPoint.y));
                src_b = cv::Point2f(src_a.x + 1, src_a.y);
                src_c = cv::Point2f(src_a.x, src_a.y + 1);
                src_d = cv::Point2f(src_a.x + 1, src_a.y + 1);

                // 计算新的像素值
                distance_to_a_x = mCorrectPoint.x - src_a.x;
                distance_to_a_y = mCorrectPoint.y - src_a.y;

                for (int channel = 0; channel < 3; channel++) {
                    drcimg.at<cv::Vec3b>(row, cols)[channel] =
                        img.at<cv::Vec3b>(src_a.y, src_a.x)[channel] * (1 - distance_to_a_x) * (1 - distance_to_a_y) +
                        img.at<cv::Vec3b>(src_b.y, src_b.x)[channel] * distance_to_a_x * (1 - distance_to_a_y) +
                        img.at<cv::Vec3b>(src_c.y, src_c.x)[channel] * distance_to_a_y * (1 - distance_to_a_x) +
                        img.at<cv::Vec3b>(src_d.y, src_d.x)[channel] * distance_to_a_y * distance_to_a_x;
                }
            }
        }

        // 裁剪
        int topCropHeight = drcimg.rows * 0.126;
        int bottomCropHeight = drcimg.rows * 0.126;
        int leftCropWidth = drcimg.cols * 0.083;
        int rightCropWidth = drcimg.cols * 0.085;

        cv::Rect roi(leftCropWidth, topCropHeight, drcimg.cols - leftCropWidth - rightCropWidth, drcimg.rows - topCropHeight - bottomCropHeight);
        cv::Mat croppedImg = drcimg(roi);

        // 将结果填充到输出的 AVFrame
        av_image_fill_arrays(frame_output->data, frame_output->linesize, croppedImg.data, AV_PIX_FMT_RGB24, croppedImg.cols, croppedImg.rows, 1);

        return true;
    }
    /**
     * Fuses two AVFrames into a single fused frame.
     *
     * This function takes two input AVFrames, frame1 and frame2, and fuses them into a single output frame, frame_fused.
     * The fusion process is performed by averaging the pixel values of the two input frames.
     * The behavior of the fusion process can be controlled by the options parameter, which specifies whether to apply
     * lens distortion correction before fusion.
     *
     * @param frame1 The first input AVFrame.
     * @param frame2 The second input AVFrame.
     * @param frame_fused The output AVFrame that will contain the fused image.
     * @param options A FusionOptions structure that contains settings for the fusion process.
     *                - apply_correction: A boolean flag that indicates whether to apply lens distortion correction
     *                  to the input frames before fusion.
     * @return True if the fusion process is successful, false otherwise.
     */
    bool image_fusion(AVFrame *frame1, AVFrame *frame2, AVFrame *frame_fused, const FusionOptions &options) {
        // 检查输入帧有效性
        if (!frame1 || !frame2 || !frame_fused) {
            return false;
        }

        cv::Mat img1, img2;

        // 根据选项决定是否进行几何校正
        if (options.apply_correction) {
            AVFrame* frame_corrected1 = av_frame_alloc();
            AVFrame* frame_corrected2 = av_frame_alloc();

            if (!frame_corrected1 || !frame_corrected2) {
                return false; 
            }

            // 进行图像几何校正
            if (!correct_image(frame1, frame_corrected1) || !correct_image(frame2, frame_corrected2)) {
                av_frame_free(&frame_corrected1);
                av_frame_free(&frame_corrected2);
                return false;
            }

            // 转换校正后的帧为 cv::Mat
            img1 = cv::Mat(frame_corrected1->height, frame_corrected1->width, CV_8UC3, frame_corrected1->data[0]);
            img2 = cv::Mat(frame_corrected2->height, frame_corrected2->width, CV_8UC3, frame_corrected2->data[0]);

            av_frame_free(&frame_corrected1);
            av_frame_free(&frame_corrected2);
        } else {
            img1 = cv::Mat(frame1->height, frame1->width, CV_8UC3, frame1->data[0]);
            img2 = cv::Mat(frame2->height, frame2->width, CV_8UC3, frame2->data[0]);
        }


        // 创建 SIFT 特征检测器
        cv::Ptr<cv::SIFT> detector = cv::SIFT::create(1000);
        std::vector<cv::KeyPoint> keypoints1, keypoints2;
        cv::Mat descriptors1, descriptors2;
        detector->detectAndCompute(img1, cv::Mat(), keypoints1, descriptors1);
        detector->detectAndCompute(img2, cv::Mat(), keypoints2, descriptors2);

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
            return false; // 透视变换失败
        }

        // 拼接图像
        int dst_width = img1.cols + img2.cols;
        int dst_height = std::max(img1.rows, img2.rows);
        cv::Mat dst = cv::Mat::zeros(dst_height, dst_width, CV_8UC3);

        // 将图像 1 复制到目标图像
        img1.copyTo(dst(cv::Rect(0, 0, img1.cols, img1.rows)));

        // 将图像 2 透视变换并复制到目标图像
        cv::Mat transformed_img2;
        cv::warpPerspective(img2, transformed_img2, homography, dst.size());

        // 自动识别重叠区域
        std::vector<cv::Point2f> corners = {
            cv::Point2f(0, 0),
            cv::Point2f(img1.cols, 0),
            cv::Point2f(img1.cols, img1.rows),
            cv::Point2f(0, img1.rows)
        };

        std::vector<cv::Point2f> transformed_corners;
        cv::perspectiveTransform(corners, transformed_corners, homography);
        cv::Rect transformed_rect = cv::boundingRect(transformed_corners);
        cv::Rect overlap_rect = cv::Rect(0, 0, img1.cols, img1.rows) & transformed_rect;

        // 进行渐入渐出法处理重叠区域
        for (int y = overlap_rect.y; y < overlap_rect.y + overlap_rect.height; y++) {
            for (int x = overlap_rect.x; x < overlap_rect.x + overlap_rect.width; x++) {
                // 计算权重
                float d1 = static_cast<float>(overlap_rect.x + overlap_rect.width - x) / overlap_rect.width; 
                float d2 = static_cast<float>(x - overlap_rect.x) / overlap_rect.width;

                // 确保权重在 [0, 1] 之间
                d1 = std::clamp(d1, 0.0f, 1.0f);
                d2 = std::clamp(d2, 0.0f, 1.0f);

                // 获取左图和右图的像素
                cv::Vec3b pixel1 = img1.at<cv::Vec3b>(y, x);
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
        cv::Rect right_non_overlap_rect(
        overlap_rect.x + overlap_rect.width, 0,
        transformed_img2.cols - overlap_rect.x - overlap_rect.width, transformed_img2.rows
        );
        if (right_non_overlap_rect.width > 0 && right_non_overlap_rect.x < dst.cols) {
            transformed_img2(right_non_overlap_rect).copyTo(dst(right_non_overlap_rect));
        }

        // 将结果保存到输出 AVFrame
        av_image_fill_arrays(frame_fused->data, frame_fused->linesize, dst.data, AV_PIX_FMT_RGB24, dst.cols, dst.rows, 1);

        return true;
        }

    void run() {
        AVFrame* frame1 = av_frame_alloc();
        AVFrame* frame2 = av_frame_alloc();
        AVFrame* frame_fused = av_frame_alloc();

        while (!quit) {
            std::unique_lock<std::mutex> lock(mutex_);

            // Add null-checks for the queues before accessing them
            if (queue_map_[0] == nullptr || queue_map_[1] == nullptr) {
                av_log(NULL, AV_LOG_ERROR, "Queue map not initialized! %p\n", this);
                return; 
            }

            // Wait until both queues have frames or until stop_ is set
            cv_.wait(lock, [&] { return (!queue_map_[0]->empty() && !queue_map_[1]->empty()) || stop_; });

            if (stop_) break;

            // Get frames from both queues
            if (!queue_map_[0]->empty() && !queue_map_[1]->empty()) {
                *frame1 = queue_map_[0]->front();
                queue_map_[0]->pop();
                *frame2 = queue_map_[1]->front();
                queue_map_[1]->pop();

                // Perform image fusion
                if (image_fusion(frame1, frame2, frame_fused)) {
                    queue_frame_fused_->push(*frame_fused);
                }
            }
        }
        // Clean up   
        // if (frame1) av_frame_free(&frame1);
        // if (frame2) av_frame_free(&frame2);
        // if (frame_fused) av_frame_free(&frame_fused);
    }
    std::shared_ptr<std::queue<AVFrame>> get_queue_frame_fused() {
        return queue_frame_fused_;
    }
private:
    std::map<int, std::shared_ptr<std::queue<AVFrame>>> queue_map_;
    std::shared_ptr<std::queue<AVFrame>> queue_frame_fused_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    bool stop_ = false;
};
#endif
