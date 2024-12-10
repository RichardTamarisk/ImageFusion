#include "include/stitcher.h"

void extractFrame(const char* filename, AVFrame*& outputFrame) {
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pCodecCtx = nullptr;
    AVPacket packet;

    // 打开视频文件
    std::cout << "尝试打开视频文件: " << filename << std::endl;
    if (avformat_open_input(&pFormatCtx, filename, nullptr, nullptr) < 0) {
        std::cerr << "无法打开视频文件: " << filename << std::endl;
        return;
    }

    // 查找流信息
    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
        std::cerr << "无法找到流信息: " << filename << std::endl;
        avformat_close_input(&pFormatCtx);
        return;
    }

    // 找到视频流
    int videoStreamIndex = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "未找到视频流: " << filename << std::endl;
        avformat_close_input(&pFormatCtx);
        return;
    }

    // 获取解码器
    AVCodecParameters *pCodecParams = pFormatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec *pCodec = avcodec_find_decoder(pCodecParams->codec_id);
    if (!pCodec) {
        std::cerr << "未找到解码器: " << filename << std::endl;
        avformat_close_input(&pFormatCtx);
        return;
    }

    // 创建解码器上下文
    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pCodecParams);

    // 打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
        std::cerr << "无法打开解码器: " << filename << std::endl;
        avcodec_free_context(&pCodecCtx);
        avformat_close_input(&pFormatCtx);
        return;
    }

    // 读取第一帧
    while (av_read_frame(pFormatCtx, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            // 发送数据包到解码器
            if (avcodec_send_packet(pCodecCtx, &packet) == 0) {
                outputFrame = av_frame_alloc();
                if (avcodec_receive_frame(pCodecCtx, outputFrame) == 0) {
                    std::cout << "成功接收帧，帧编号: " << outputFrame->pts << std::endl;
                    av_packet_unref(&packet);
                    break;  // 成功提取帧后退出循环
                }
                av_frame_free(&outputFrame);
            }
            av_packet_unref(&packet);
        }
    }

    // 清理
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <video1.mp4> <video2.mp4>" << std::endl;
        return -1;
    }

    AVFrame *avFrame1 = nullptr;
    AVFrame *avFrame2 = nullptr;

    // 提取第一帧
    extractFrame(argv[1], avFrame1);
    extractFrame(argv[2], avFrame2);

    // 检查是否成功提取帧
    if (!avFrame1 || !avFrame2) {
        std::cerr << "提取帧失败。" << std::endl;
        return -1;
    }

    // 将 AVFrame 转换为 cv::Mat
    cv::Mat mat1 = avframeToCvmat(avFrame1);
    cv::Mat mat2 = avframeToCvmat(avFrame2);

    cv::imwrite("frame1.jpg", mat1);

    // 检查转换是否成功
    if (mat1.empty() || mat2.empty()) {
        std::cerr << "图像转换失败。" << std::endl;
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1;
    }

    // 将 cv::Mat 转换回 AVFrame
    AVFrame *newFrame1 = cvmatToAvframe(&mat1, nullptr);
    AVFrame *newFrame2 = cvmatToAvframe(&mat2, nullptr);

    cv::imwrite("frame2.jpg", avframeToCvmat(newFrame2));

    // 检查转换回 AVFrame 是否成功
    if (!newFrame1 || !newFrame2) {
        std::cerr << "AVFrame 转换失败。" << std::endl;
        av_frame_free(&avFrame1);
        av_frame_free(&avFrame2);
        return -1;
    }

    // 清理内存
    av_frame_free(&avFrame1);
    av_frame_free(&avFrame2);
    av_frame_free(&newFrame1);
    av_frame_free(&newFrame2);

    return 0;
}