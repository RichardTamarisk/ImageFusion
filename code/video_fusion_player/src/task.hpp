#ifndef TASK_HPP
#define TASK_HPP

#include "common.h"

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
        av_log(NULL, AV_LOG_ERROR, "Task init! %p\n", this);
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
     * Fuses two AVFrames into a single fused frame.
     *
     * This function takes two input AVFrames, frame1 and frame2, and fuses them into a single output frame, frame_fused.
     * The fusion process is performed by averaging the pixel values of the two input frames.
     *
     * @param frame1 The first input AVFrame.
     * @param frame2 The second input AVFrame.
     * @param frame_fused The output AVFrame that will contain the fused image.
     * @return True if the fusion process is successful, false otherwise.
     */
    bool image_fusion(AVFrame *frame1, AVFrame *frame2, AVFrame *frame_fused) {
        if (frame1->width != frame2->width || frame1->height != frame2->height) {
            av_log(NULL, AV_LOG_ERROR, "Frame sizes do not match for fusion!\n");
            return false;
        }

        // Convert AVFrame to cv::Mat
        cv::Mat img1(frame1->height, frame1->width, CV_8UC3, frame1->data[0]); 
        cv::Mat img2(frame2->height, frame2->width, CV_8UC3, frame2->data[0]); 

        // Create the SIFT feature detector
        cv::Ptr<cv::SIFT> detector = cv::SIFT::create();

        // Detect SIFT key points and descriptors
        std::vector<cv::KeyPoint> keypoints1, keypoints2;
        cv::Mat descriptors1, descriptors2;
        detector->detectAndCompute(img1, cv::Mat(), keypoints1, descriptors1);
        detector->detectAndCompute(img2, cv::Mat(), keypoints2, descriptors2);

        // Create a Flann-based descriptor matcher
        cv::Ptr<cv::DescriptorMatcher> matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
        std::vector<std::vector<cv::DMatch>> matches;
        matcher->knnMatch(descriptors1, descriptors2, matches, 2);

        // Screen out good matching point pairs
        std::vector<cv::DMatch> good_matches;
        for (size_t i = 0; i < matches.size(); i++) {
            if (matches[i][0].distance < 0.7 * matches[i][1].distance) {
                good_matches.push_back(matches[i][0]);
            }
        }

        // Extract the key points of the match
        std::vector<cv::Point2f> points1, points2;
        for (const auto& match : good_matches) {
            points1.push_back(keypoints1[match.queryIdx].pt);
            points2.push_back(keypoints2[match.trainIdx].pt);
        }

        // The RANSAC algorithm was used to compute the homologous transformation matrix
        cv::Mat homography = cv::findHomography(points2, points1, cv::RANSAC);

        // perspective transformation
        cv::Mat dst;
        cv::warpPerspective(img2, dst, homography, cv::Size(img1.cols + img2.cols, img1.rows));

        // Splice image A onto the perspective transform result
        cv::Rect roi_rect = cv::Rect(0, 0, img1.cols, img1.rows);
        img1.copyTo(dst(roi_rect));

        // Convert the spliced image to AVFrame
        // It is assumed that the frame fused is the same size as dst
        frame_fused->width = dst.cols;
        frame_fused->height = dst.rows;
    
        // The dst data needs to be copied into the frame fused
        // Suppose the frame fused data format is CV 8UC3
        memcpy(frame_fused->data[0], dst.data, dst.total() * dst.elemSize());

        // Copy the basic properties of frame1 to the fused frame
        copyFrame(frame1, frame_fused);

        // // Perform pixel-wise averaging (assuming YUV420P format)
        // for (int y = 0; y < frame1->height; y++) {
        //     for (int x = 0; x < frame1->width; x++) {
        //         // Y plane (grayscale)
        //         frame_fused->data[0][y * frame_fused->linesize[0] + x] =
        //             (frame1->data[0][y * frame1->linesize[0] + x] + frame2->data[0][y * frame2->linesize[0] + x]) / 2;
        //     }
        // }

        // // UV planes (color)
        // for (int y = 0; y < frame1->height / 2; y++) {
        //     for (int x = 0; x < frame1->width / 2; x++) {
        //         frame_fused->data[1][y * frame_fused->linesize[1] + x] =
        //             (frame1->data[1][y * frame1->linesize[1] + x] + frame2->data[1][y * frame2->linesize[1] + x]) / 2;
        //         frame_fused->data[2][y * frame_fused->linesize[2] + x] =
        //             (frame1->data[2][y * frame1->linesize[2] + x] + frame2->data[2][y * frame2->linesize[2] + x]) / 2;
        //     }
        // }

        return true;
    }

    void run() {
        AVFrame* frame1 = av_frame_alloc();
        AVFrame* frame2 = av_frame_alloc();
        AVFrame* frame_fused = av_frame_alloc();

        while (true) {
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
        av_frame_free(&frame1);
        av_frame_free(&frame2);
        av_frame_free(&frame_fused);
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
