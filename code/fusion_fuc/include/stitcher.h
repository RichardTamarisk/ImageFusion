#ifndef STITCHER_H
#define STITCHER_H

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <opencv2/features2d.hpp>
#include <chrono>
#include <opencv2/calib3d.hpp>
#include <opencv2/opencv.hpp>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/samplefmt.h>

cv::Mat avframeToCvmat(const AVFrame *frame);

AVFrame *cvmatToAvframe(const cv::Mat *image, AVFrame *frame);

bool correct_image(AVFrame *frame_input, AVFrame *frame_output);

bool image_fusion(AVFrame *frame1, AVFrame *frame2, AVFrame *frame_fused, bool is_correct);

#endif // STITCHER_H