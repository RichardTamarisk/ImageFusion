#include "include/stitcher.h"


int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_left file> <input_right file>" << std::endl;
        return -1;
    }

    const char* videoFile1 = argv[1];
    const char* videoFile2 = argv[2];

    // 视频文件处理
    AVFormatContext* formatContext1 = nullptr;
    AVFormatContext* formatContext2 = nullptr;
    AVFrame* frame1 = nullptr;
    AVFrame* frame2 = nullptr;
    AVCodecContext* codecContext1 = nullptr;
    AVCodecContext* codecContext2 = nullptr;
    const AVCodec* codec1 = nullptr;
    const AVCodec* codec2 = nullptr;
    int videoStreamIndex1 = -1;
    int videoStreamIndex2 = -1;

    // 打开第一个视频文件
    if (avformat_open_input(&formatContext1, videoFile1, nullptr, nullptr) < 0) {
        std::cerr << "Cannot open video file: " << videoFile1 << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(formatContext1, nullptr) < 0) {
        avformat_close_input(&formatContext1);
        std::cerr << "Cannot get video information" << std::endl;
        return -1;
    }

    // 查找视频流并打开解码器
    for (unsigned int i = 0; i < formatContext1->nb_streams; i++) {
        if (formatContext1->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex1 = i;
            codec1 = avcodec_find_decoder(formatContext1->streams[i]->codecpar->codec_id);
            if (!codec1) {
                std::cerr << "Cannot find video decoder" << std::endl;
                avformat_close_input(&formatContext1);
                return -1;
            }
            codecContext1 = avcodec_alloc_context3(codec1);
            avcodec_parameters_to_context(codecContext1, formatContext1->streams[i]->codecpar);
            if (avcodec_open2(codecContext1, codec1, nullptr) < 0) {
                std::cerr << "Cannot open video decoder" << std::endl;
                avcodec_free_context(&codecContext1);
                avformat_close_input(&formatContext1);
                return -1;
            }
            break;
        }
    }

    if (videoStreamIndex1 == -1) {
        avformat_close_input(&formatContext1);
        std::cerr << "Did not find video stream" << std::endl;
        return -1;
    }

    // 读取第一帧
    AVPacket packet1;
    frame1 = av_frame_alloc();
    if (av_read_frame(formatContext1, &packet1) >= 0) {
        if (packet1.stream_index == videoStreamIndex1) {
            avcodec_send_packet(codecContext1, &packet1);
            if (avcodec_receive_frame(codecContext1, frame1) >= 0) {
                av_packet_unref(&packet1);
            }
        }
        av_packet_unref(&packet1);
    }

    // 关闭第一个视频文件
    avcodec_free_context(&codecContext1);
    avformat_close_input(&formatContext1);

    // 打开第二个视频文件
    if (avformat_open_input(&formatContext2, videoFile2, nullptr, nullptr) < 0) {
        std::cerr << "Cannot open video file: " << videoFile2 << std::endl;
        av_frame_free(&frame1);
        return -1;
    }

    if (avformat_find_stream_info(formatContext2, nullptr) < 0) {
        avformat_close_input(&formatContext2);
        std::cerr << "Cannot get video information" << std::endl;
        av_frame_free(&frame1);
        return -1;
    }

    // 查找视频流并打开解码器
    for (unsigned int i = 0; i < formatContext2->nb_streams; i++) {
        if (formatContext2->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex2 = i;
            codec2 = avcodec_find_decoder(formatContext2->streams[i]->codecpar->codec_id);
            if (!codec2) {
                std::cerr << "Cannot find video decoder" << std::endl;
                avformat_close_input(&formatContext2);
                av_frame_free(&frame1);
                return -1;
            }
            codecContext2 = avcodec_alloc_context3(codec2);
            avcodec_parameters_to_context(codecContext2, formatContext2->streams[i]->codecpar);
            if (avcodec_open2(codecContext2, codec2, nullptr) < 0) {
                std::cerr << "Cannot open video decoder" << std::endl;
                avcodec_free_context(&codecContext2);
                avformat_close_input(&formatContext2);
                av_frame_free(&frame1);
                return -1;
            }
            break;
        }
    }

    if (videoStreamIndex2 == -1) {
        avformat_close_input(&formatContext2);
        av_frame_free(&frame1);
        std::cerr << "Did not find video stream" << std::endl;
        return -1;
    }

    // 读取第二帧
    AVPacket packet2;
    frame2 = av_frame_alloc();
    if (av_read_frame(formatContext2, &packet2) >= 0) {
        if (packet2.stream_index == videoStreamIndex2) {
            avcodec_send_packet(codecContext2, &packet2);
            if (avcodec_receive_frame(codecContext2, frame2) >= 0) {
                av_packet_unref(&packet2);
            }
        }
        av_packet_unref(&packet2);
    }

    // 关闭第二个视频文件
    avcodec_free_context(&codecContext2);
    avformat_close_input(&formatContext2);

    // 检查 AVFrame 是否成功分配
    if (!frame1 || !frame2) {
        std::cerr << "Cannot allocate AVFrame" << std::endl;
        av_frame_free(&frame1);
        av_frame_free(&frame2);
        return -1;
    }

    // 创建输出帧
    AVFrame* frame_fused = av_frame_alloc();
    if (!frame_fused) {
        std::cerr << "Cannot allocate output frame" << std::endl;
        av_frame_free(&frame1);
        av_frame_free(&frame2);
        return -1;
    }

    // 调用 image_fusion 函数
    bool success = image_fusion(frame1, frame2, frame_fused, true);
    if (success) {
        std::cout << "Image fusion success!" << std::endl;
        std::cout << "Output file: build/fused.jpg" << std::endl;
        // 处理输出帧，如保存或显示
    } else {
        std::cerr << "Image fusion failed!" << std::endl;
    }

    // 释放资源
    av_frame_free(&frame1);
    av_frame_free(&frame2);
    av_frame_free(&frame_fused);

    return 0;
}