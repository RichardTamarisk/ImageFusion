#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>

#define SDL_AUDIO_BUFFER_SIZE 1024

// SDL 相关变量
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;

// 错误处理
void log_error(const char *msg) 
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

// 初始化 SDL
void init_sdl(int width, int height) 
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) 
    {
        log_error("Could not initialize SDL");
    }
    window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    if (!window) 
    {
        log_error("Could not create window");
    }
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) 
    {
        log_error("Could not create renderer");
    }
}

// 释放 SDL 资源
void cleanup_sdl() 
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// 播放视频
void play_video(const char *filename) 
{
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVPacket packet;

    // 打开视频文件
    if (avformat_open_input(&format_ctx, filename, NULL, NULL) < 0) 
    {
        log_error("Could not open video file");
    }

    // 查找视频流
    if (avformat_find_stream_info(format_ctx, NULL) < 0) 
    {
        log_error("Could not find stream information");
    }

    // 查找第一个视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) 
    {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
        {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) 
    {
        log_error("Could not find video stream");
    }

    // 获取视频解码器
    codec = avcodec_find_decoder(format_ctx->streams[video_stream_index]->codecpar->codec_id);
    if (!codec) 
    {
        log_error("Unsupported codec");
    }

    // 创建解码上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) 
    {
        log_error("Could not allocate codec context");
    }

    // 复制参数
    if (avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_index]->codecpar) < 0) 
    {
        log_error("Could not copy parameters to codec context");
    }

    // 打开解码器
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) 
    {
        log_error("Could not open codec");
    }

    // 创建 SDL 窗口
    init_sdl(codec_ctx->width, codec_ctx->height);

    // 创建帧
    frame = av_frame_alloc();
    if (!frame) 
    {
        log_error("Could not allocate frame");
    }

    // 主循环
    while (av_read_frame(format_ctx, &packet) >= 0) 
    {
        if (packet.stream_index == video_stream_index) 
        {
            // 解码帧
            if (avcodec_send_packet(codec_ctx, &packet) < 0) 
            {
                log_error("Error sending packet to decoder");
            }

            while (avcodec_receive_frame(codec_ctx, frame) == 0) 
            {
                // 创建纹理
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, codec_ctx->width, codec_ctx->height);
                if (!texture) 
                {
                    log_error("Could not create texture");
                }

                // 更新纹理
                SDL_UpdateYUVTexture(texture, NULL,
                                      frame->data[0], frame->linesize[0],
                                      frame->data[1], frame->linesize[1],
                                      frame->data[2], frame->linesize[2]);

                // 渲染纹理
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

                // 控制播放速度
                SDL_Delay(1000 / 25); // 假设帧率为 25
            }
        }
        av_packet_unref(&packet);
    }

    // 释放资源
    av_frame_free(&frame);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    cleanup_sdl();
}

int main(int argc, char *argv[]) 
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
        return 1;
    }

    play_video(argv[1]);

    return 0;
}