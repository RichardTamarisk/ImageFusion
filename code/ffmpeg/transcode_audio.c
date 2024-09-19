#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>

// output bit rate 
#define OUTPUT_BIT_RATE 96000
// number of output channels
#define OUTPUT_CHANNELS 2


static int decode(AVCodecContext *ctx, AVFrame *frame, AVFrame *outFrame, AVPacket *inPkt, const char *fileName)
{
    int ret = -1;
    char buffer[1024];

    // send packet to decoder
    ret = avcodec_send_packet(ctx, inPkt);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to send packet to decoder for file\n");
        return 0;
    }

    while(ret >= 0)
    {
        ret = avcodec_receive_frame(ctx, frame);
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return 0;
        } else if(ret < 0)
        {
            return -1;
        }

        // encode
        for(size_t i = 0; i < 3; i++)
            outFrame->data[i] = frame->data[i];
        outFrame->pts = frame->pts;
        
        if(inPkt)
            av_packet_unref(inPkt);
    }
}

static void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *file)
{
    int ret;

    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) 
    {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) 
    {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) 
        {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }

        fwrite(pkt->data, 1, pkt->size, file);
        av_packet_unref(pkt);
    }
}

int main(int argc, char **argv)
{
    int ret = -1;

    char *src;
    char *dst;

    AVFormatContext *pFmtCtx = NULL;

    AVStream *inStream = NULL;

    AVPacket *inPkt = NULL;
    AVPacket *outPkt = NULL;

    const AVCodec *inCodec = NULL;
    const AVCodec *outCodec = NULL;

    AVCodecContext *inCodecCtx = NULL;
    AVCodecContext *outCodecCtx = NULL;

    AVFrame *inFrame = NULL;
    AVFrame *outFrame = NULL;

    int audio_stream_index = -1;
    int video_stream_index = -1;

    av_log_set_level(AV_LOG_DEBUG);
    if(argc < 3)
    {
        av_log(NULL, AV_LOG_ERROR, "Usage: transcode_audio_2 <input file> <output file>\n");
        exit(1);
    }

    src = argv[1];
    dst = argv[2];

    if((ret = avformat_open_input(&pFmtCtx, src, NULL, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not open input file\n");
        exit(1);
    }

    // find audio stream index
    for(int i = 0; i < pFmtCtx->nb_streams; i++)
    {
        if(pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_index = i;
        } else if(pFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) 
        {
            video_stream_index = i;
        }
    }

    if(audio_stream_index == -1)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find audio stream in input file\n");
        avformat_close_input(&pFmtCtx);
        exit(1);
    }

    if((ret = avformat_find_stream_info(pFmtCtx, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find stream information\n");
        goto end;
    }

    inStream = pFmtCtx->streams[audio_stream_index];

    // find the decoder
    inCodec = avcodec_find_decoder(inStream->codecpar->codec_id);
    if(!inCodec)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to find decoder\n");
        goto end;
    }

    // init decoder context
    inCodecCtx = avcodec_alloc_context3(inCodec);
    if(!inCodecCtx)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate decoder context\n");
        goto end;
    }

    avcodec_parameters_to_context(inCodecCtx, inStream->codecpar);

    // bind decoder and decoder context
    ret = avcodec_open2(inCodecCtx, inCodec, NULL);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open decoder\n");
        goto end;
    }

    // create AVFrame
    inFrame = av_frame_alloc();
    if(!inFrame)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate input frame\n");
        goto end;
    }

    // create AVPacket
    inPkt = av_packet_alloc();
    if(!inPkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate input packet\n");
        goto end;
    }

    // find the encoder by ID
    outCodec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if(!outCodec)
    {
        av_log(NULL, AV_LOG_ERROR, "Couldn't find codec\n");
        goto end;
    }

    // init encoder context
    outCodecCtx = avcodec_alloc_context3(outCodec);
    if(!outCodecCtx)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate encoder context\n");
    }

    if(inCodecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        // outCodecCtx->sample_fmt = inCodecCtx->sample_fmt;
        // if(outCodec->id == AV_CODEC_ID_MP2)
        // {
        //     outCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
        // } else if(outCodec->id == AV_CODEC_ID_MP3)
        // {
        //     outCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        // }
        // outCodecCtx->bit_rate = inCodecCtx->bit_rate;
        outCodecCtx->bit_rate = OUTPUT_BIT_RATE;
        // outCodecCtx->sample_rate = inCodecCtx->sample_rate;
        // outCodecCtx->ch_layout = inCodecCtx->ch_layout;
        outCodecCtx->time_base = inCodecCtx->time_base;

    }

    ret = avcodec_open2(outCodecCtx, outCodec, NULL);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open encoder\n");
        goto end;
    }

    FILE *f = fopen(dst, "wb");
    if(!f)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to open output file\n");
        goto end;
    }

    // create AVFrame
    outFrame = av_frame_alloc();
    if(!outFrame)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate output frame\n");
        goto end;
    }

    // 设置输出参数
    outFrame->nb_samples = inCodecCtx->frame_size; // 设置默认样本数
    outFrame->format = outCodecCtx->sample_fmt; // 使用输出编码器的样本格式
    ret = av_channel_layout_copy(&outFrame->ch_layout, &outCodecCtx->ch_layout);
    if (ret < 0) 
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy channel layout\n");
        goto end;
    }

    // 分配输出帧缓冲区
    ret = av_frame_get_buffer(outFrame, 0);
    if (ret < 0) 
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate output frame buffer\n");
        goto end;
    }

    // create AVPacket 
    outPkt = av_packet_alloc();
    if(!outPkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate output packet\n");
        goto end;
    }

    // read audio data from input file and encode it
    while(av_read_frame(pFmtCtx, inPkt) >= 0)
    {
        // printf("start decoding frame\n");
        if (inPkt->stream_index == audio_stream_index)
        {
            decode(inCodecCtx, inFrame, outFrame, inPkt, src);

            /*
            * set output frame size to 1152
            * because mp2 encoder respect the frame size is 1152
            */ 
            outFrame->nb_samples = 1152; // 

            // deal with the case that input frame size is less than output frame size
            if (inFrame->nb_samples < outFrame->nb_samples) 
            {
                // fill the rest of output frame with zero value
                for (int i = inFrame->nb_samples; i < outFrame->nb_samples; i++) 
                {
                    for (int channel = 0; channel < outCodecCtx->channels; channel++) 
                    {
                        // fill the rest of output frame with zero value
                        outFrame->data[channel][i] = 0;
                    }
                }
            } else 
            {
                // copy input frame data to output frame data
                for (int channel = 0; channel < outCodecCtx->channels; channel++) 
                {
                    memcpy(outFrame->data[channel], inFrame->data[channel], inFrame->nb_samples * sizeof(int16_t));
                }
            }
            encode(outCodecCtx, outFrame, outPkt, f);
        }
    }

    // write the buffered frame
    decode(inCodecCtx, inFrame, outFrame, NULL, src);
    encode(outCodecCtx, outFrame, outPkt, f);


end:
    if(pFmtCtx)
    {
        avformat_close_input(&pFmtCtx);
        pFmtCtx = NULL;
    }   
    if(inCodecCtx)
    {
        avcodec_free_context(&inCodecCtx);
        inCodecCtx = NULL;
    }
    if(inFrame)
    {
        av_frame_free(&inFrame);
        inFrame = NULL;
    }
    if(inPkt)
    {
        av_packet_free(&inPkt);
        inPkt = NULL;
    }

    return 0;
}