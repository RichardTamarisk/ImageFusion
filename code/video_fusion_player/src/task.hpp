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
