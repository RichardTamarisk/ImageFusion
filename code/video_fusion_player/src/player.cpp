/*
 * copyright (c) 2024 Jack Lau
 * 
 * This file is a tutorial that fuse and render video through ffmpeg and SDL API 
 * 
 * FFmpeg version 5.1.4
 * SDL2 version 2.30.3
 *
 * Tested on MacOS 14.1.2, compiled with clang 14.0.3
 */
#include "common.h"
#define ONESECOND 1000

#include "stream_context.hpp"
#include "task.hpp"

typedef struct VideoState {
    // AVCodecContext *avctx;
    // AVPacket       *pkt;
    // AVFrame        *frame;
    // AVStream       *stream;
    int            frameRate;
    SDL_Texture    *texture;
}VideoState;

static int w_width = 1620;
static int w_height = 1080;

static SDL_Window *win = NULL;
static SDL_Renderer *renderer = NULL;

static void render_frame(SDL_Texture *texture, AVFrame *frame, int frameRate) {
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "render with invaild frame!\n");
        return;
    }

    SDL_UpdateYUVTexture(texture, NULL,
                         frame->data[0], frame->linesize[0] ,
                         frame->data[1], frame->linesize[1],
                         frame->data[2], frame->linesize[2]);
    
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    // int frameRate = is->stream->r_frame_rate.num/is->stream->r_frame_rate.den;
    Uint32 delayTime = (Uint32)(ONESECOND/frameRate);
    if(frameRate <= 0){
        av_log(NULL, AV_LOG_ERROR,  "Failed to get framerate!\n");
        SDL_Delay(33);
        return;
    }
    av_log(NULL, AV_LOG_DEBUG, "delayTime: %d\n", delayTime);
    SDL_Delay(delayTime);
    return;
}

// Thread function to handle SDL events and rendering
static void sdl_render_thread(std::shared_ptr<Task> task, SDL_Texture *texture, int frameRate) {
    SDL_Event event;
    auto fused_frame_queue = task->get_queue_frame_fused();

    while (!quit) {
        // Render frames from fused queue
        if (!fused_frame_queue->empty()) {
            AVFrame frame = fused_frame_queue->front();
            fused_frame_queue->pop();

            texture = SDL_CreateTexture(renderer, 
                                SDL_PIXELFORMAT_IYUV, 
                                SDL_TEXTUREACCESS_STREAMING, 
                                frame.width, 
                                frame.height);
                    
            if (!texture) {
                av_log(NULL, AV_LOG_ERROR, "Failed to create texture: %s\n", SDL_GetError());
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(win);
                SDL_Quit();
                quit = true;
            }

            render_frame(texture, &frame, frameRate); // Assuming 30 FPS for now
        }

        // Poll for SDL events
        // while (SDL_PollEvent(&event)) {
        //     if (event.type == SDL_QUIT) {
        //         quit = true;
        //         break;
        //     }
        // }

        // SDL_Delay(10); // Reduce CPU usage
    }
}

// Function to be run on the main thread for event handling
void sdl_event_thread() {
    SDL_Event event;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                quit = true;
                break;
            }
            // Handle other events...
        }
        SDL_Delay(10); // Reduce CPU usage
    }
}

int main(int argc, char *argv[])
{

    int ret = -1;
    SDL_Texture *texture = NULL;
    SDL_Event event;
    Uint32 pixformat = 0;

    // Deal with arguments
    av_log_set_level(AV_LOG_INFO);
    if(argc < 3){
        av_log(NULL, AV_LOG_ERROR, "the arguments must be more than 3!\n");
        exit(-1);
    }

    std::shared_ptr<Task> task = std::make_shared<Task>(2);
    auto sc_0 = std::make_shared<StreamContext>(0, argv[1], task);
    auto sc_1 = std::make_shared<StreamContext>(1, argv[2], task);
    
    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO)){
        fprintf(stderr, "Couldn't initialize SDL - %s\n", (SDL_GetError()));
        return -1;
    }
    // Create window from SDL
    win = SDL_CreateWindow("simple player",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           w_width, w_height,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!win) {
        fprintf(stderr, "Failed to create window, %s\n", SDL_GetError());
        return -1;
    }   
    renderer = SDL_CreateRenderer(win, -1, 0);

    int frameRate = sc_0->stream->r_frame_rate.num / sc_1->stream->r_frame_rate.den;
    
    // Launch decoding threads for both video streams
    std::thread decode_thread_1(&StreamContext::decode_loop, sc_0);
    std::thread decode_thread_2(&StreamContext::decode_loop, sc_1);
    // Start the rendering thread with parameters (task and texture)
    std::thread render_thread(sdl_render_thread, task, texture, frameRate);
    // Handle events on the main thread
    // sdl_event_thread();
    // Wait for decoding threads to finish
    decode_thread_1.join();
    decode_thread_2.join();


    // sdl_render_thread(task, texture, frameRate);
    sdl_event_thread();

    // Wait for rendering thread to finish before exiting
    render_thread.join();

    // Cleanup resources
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
    return EXIT_SUCCESS;
}