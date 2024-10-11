#ifndef STREAM_CONTEXT_HPP
#define STREAM_CONTEXT_HPP

#include "common.h"
#include "task.hpp"

class StreamContext {
private:
    int id;
    std::shared_ptr<Task> task;
public:
    StreamContext(int id,  char *filePath, std::shared_ptr<Task> task)
        : id(id), filePath(filePath), task(task) {
        fmtCtx = NULL;
        stream = NULL;
        decodec = NULL;
        ctx = NULL;
        pkt = av_packet_alloc();
        frame = av_frame_alloc();
        idx = -1;
        // init
        open_media(this);
    }
    int open_media(StreamContext *sc) {
        int ret = -1;
        //open multimedia file and get stream info
        if( (ret = avformat_open_input(&sc->fmtCtx, sc->filePath, NULL, NULL)) < 0 ) {
            av_log(NULL, AV_LOG_ERROR, " %s \n", av_err2str(ret));
            goto end;
        }
        if((ret = avformat_find_stream_info(sc->fmtCtx, NULL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "%s\n", av_err2str(ret));
            goto end;
        }
        //find the best stream
        if((sc->idx = av_find_best_stream(sc->fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0) {
            av_log(sc->fmtCtx, AV_LOG_ERROR, "There is no audio stream!\n");
            goto end;
        }
        //get decodec by codec_id from stream info
        sc->stream = sc->fmtCtx->streams[sc->idx];
        sc->decodec = avcodec_find_decoder(sc->stream->codecpar->codec_id);
        if(!sc->decodec){
            av_log(NULL, AV_LOG_ERROR, "Couldn't find codec: libx264 \n");
            goto end;
        }
        //init decoder context
        sc->ctx = avcodec_alloc_context3(sc->decodec);
        if(!sc->ctx){
            av_log(NULL, AV_LOG_ERROR, "No memory!\n");
            goto end;
        }
        //copy parameters 
        avcodec_parameters_to_context(sc->ctx, sc->stream->codecpar);

        //bind decoder and decoder context
        ret = avcodec_open2(sc->ctx, sc->decodec, NULL);
        if(ret < 0){
            av_log(NULL, AV_LOG_ERROR, "Couldn't open the codec: %s\n", av_err2str(ret));
            goto end;
        }
    end:
        return ret;
    }
    int get_id() { return id; }
    int decode_loop() {
        int ret = -1;
        while(av_read_frame(fmtCtx, pkt) >= 0 && !quit){
            if(pkt->stream_index == idx ){
                //decode
                ret = decode();
                if(ret < 0){
                    av_log(NULL, AV_LOG_ERROR, "Failed to decode frame!\n");
                    return ret;
                }
            }
            av_packet_unref(pkt);
        }
        if (!quit) {
            decode();
            av_log(NULL, AV_LOG_INFO, "No more packet!\n");
            // there is no more packet
            // quit = true;
        }
        return ret;
    }
    int decode() {
        int ret = -1;

        char buffer[1024];
        // Send packet to decoder
        ret = avcodec_send_packet(ctx, pkt);
        if(ret < 0){
            av_log(NULL, AV_LOG_ERROR, "Failed to send frame to decoder!\n");
            return ret;
        }

        while (ret >= 0)
        {
            ret = avcodec_receive_frame(ctx, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
                ret = 0;
                return ret;
            }else if(ret < 0){
                ret = -1;
                return ret;
            }
            // Fill AVFrame to task
            task->fill_queue(id, frame);
            // render(is);
        }
        return ret;
    }
    AVFormatContext *fmtCtx;
    AVStream        *stream;
    AVCodecContext  *ctx;
    const AVCodec   *decodec;
    AVPacket        *pkt;
    AVFrame         *frame;
    
    int             width;
    int             height;
    int             idx;
    char            *filePath;
};

#endif