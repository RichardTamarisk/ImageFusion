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
     * Fuses two AVFrames into a single fused frame.
     *
     * This function takes two input AVFrames, frame1 and frame2, and fuses them into a single output frame, frame_fused.
     * The fusion process is performed by averaging the pixel values of the two input frames.
     *
     * @param frame1 The first input AVFrame.
     * @param frame2 The second input AVFrame.
     * @param frame_fused The output AVFrame that will contain the fused image.
     * @param is_correct The boolean value will determine whether to perform image correction
     * @return True if the fusion process is successful, false otherwise.
     */
    // This function declare here just to show interface design
    // bool image_fusion(AVFrame *frame1, AVFrame *frame2, AVFrame *frame_fused, bool is_correct){}

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
                if (image_fusion(frame1, frame2, frame_fused, true)) {
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