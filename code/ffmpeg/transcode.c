#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
static AVFormatContext *video_ofmt_ctx;
static AVFormatContext *audio_ofmt_ctx;

typedef struct StreamContext 
{
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;

    AVFrame *dec_frame;
} StreamContext;
static StreamContext *stream_ctx;

static int open_input_file(const char *filename)
{
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) 
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) 
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    stream_ctx = av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodecContext *codec_ctx;
        const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!dec) 
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }

        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) 
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }

        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) 
        {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                   "for stream #%u\n", i);
            return ret;
        }

        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
        {
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);

            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) 
            {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
        stream_ctx[i].dec_ctx = codec_ctx;

        stream_ctx[i].dec_frame = av_frame_alloc();

        if(stream_ctx[i].dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            stream_ctx[i].dec_frame->format = stream_ctx[i].dec_ctx->pix_fmt;
            stream_ctx[i].dec_frame->width = stream_ctx[i].dec_ctx->width;
            stream_ctx[i].dec_frame->height = stream_ctx[i].dec_ctx->height;
        }
        else if(stream_ctx[i].dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            stream_ctx[i].dec_frame->format = stream_ctx[i].dec_ctx->sample_fmt;
            stream_ctx[i].dec_frame->nb_samples = stream_ctx[i].dec_ctx->frame_size;
            if(av_channel_layout_copy(&stream_ctx[i].dec_frame->ch_layout, &stream_ctx[i].dec_ctx->channel_layout) < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy channel layout\n");
                return AVERROR(EINVAL);
            }

        }


        if (!stream_ctx[i].dec_frame)
            return AVERROR(ENOMEM);
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    const AVCodec *encoder;
    int ret;
    unsigned int i;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) 
    {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }


    for (i = 0; i < ifmt_ctx->nb_streams; i++) 
    {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) 
        {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
        {
            /* in this example, we choose transcoding to same codec */
            // encoder = avcodec_find_encoder(dec_ctx->codec_id);
            if(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                encoder = avcodec_find_encoder(dec_ctx->codec_id);
            }
            else if(dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                // encoder = avcodec_find_encoder(AV_CODEC_ID_MP2);
                encoder = avcodec_find_encoder(dec_ctx->codec_id);
            }

            if (!encoder) 
            {
                av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }

            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) 
            {
                av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }

            /* In this example, we transcode to same properties (picture size,
             * sample rate etc.). These properties can be changed for output
             * streams easily using filters */
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) 
            {
                enc_ctx->height = dec_ctx->height;
                enc_ctx->width = dec_ctx->width;
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                /* take first format from list of supported formats */
                if (encoder->pix_fmts)
                    enc_ctx->pix_fmt = encoder->pix_fmts[0];
                else
                    enc_ctx->pix_fmt = dec_ctx->pix_fmt;
                /* video time_base can be set to whatever is handy and supported by encoder */
                enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
                enc_ctx->time_base = dec_ctx->time_base;
                enc_ctx->bit_rate = dec_ctx->bit_rate;
            } else 
            {
                // Audio
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
                if (ret < 0)
                    return ret;
                /* take first format from list of supported formats */
                if(encoder->sample_fmts)
                {
                    enc_ctx->sample_fmt = encoder->sample_fmts[0];
                } else
                {
                    enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
                }
                enc_ctx->time_base = dec_ctx->time_base;
            }

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) 
            {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) 
            {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
                return ret;
            }

            out_stream->time_base = in_stream->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) 
        {
            av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else 
        {
            /* if this stream must be remuxed */
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) 
            {
                av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }

    }
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) 
        {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}


static int encode_write_frame(unsigned int stream_index, int flush)
{
    StreamContext *stream = &stream_ctx[stream_index];
    AVFrame *dec_frame = flush ? NULL : stream->dec_frame; // 使用解码后的帧
    AVPacket *enc_pkt = av_packet_alloc();
    int ret;
    
    ret = avcodec_send_frame(stream->enc_ctx, dec_frame);
    if (ret < 0)
        return ret;

    while (1) 
    {
        ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0; 
        
        // set packet pts and stream_index
        enc_pkt->stream_index = stream_index;
        if(dec_frame)
        {
            enc_pkt->pts = dec_frame->pts;
        }

        
        av_packet_rescale_ts(enc_pkt, stream->enc_ctx->time_base, ofmt_ctx->streams[stream_index]->time_base);

        av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");

        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        av_packet_unref(enc_pkt);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int flush_encoder(unsigned int stream_index)
{
    if (!(stream_ctx[stream_index].enc_ctx->codec->capabilities &
                AV_CODEC_CAP_DELAY))
        return 0;

    av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder %s\n", stream_index, stream_ctx[stream_index].enc_ctx->codec->name);
    return encode_write_frame(stream_index, 1);
}

int main(int argc, char **argv)
{
    int ret;
    AVPacket *packet = NULL;
    unsigned int stream_index;
    unsigned int i;

    if (argc != 3) 
    {
        av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n", argv[0]);
        return 1;
    }

    if ((ret = open_input_file(argv[1])) < 0)
        goto end;
    if ((ret = open_output_file(argv[2])) < 0)
        goto end;
    if (!(packet = av_packet_alloc()))
        goto end;

    /* read all packets */
    while (1) 
    {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;

        stream_index = packet->stream_index;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", stream_index);

        av_packet_rescale_ts(packet, ifmt_ctx->streams[stream_index]->time_base, stream_ctx[stream_index].dec_ctx->time_base);

        // ret = avcodec_send_packet(stream_ctx[stream_index].dec_ctx, packet);
        av_log(NULL, AV_LOG_DEBUG, "Sending packet of type %s to decoder\n", stream_ctx[stream_index].dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ? "video" : "audio");
        if (stream_ctx[stream_index].dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) 
        {
            // video
            ret = avcodec_send_packet(stream_ctx[stream_index].dec_ctx, packet);
        } else if (stream_ctx[stream_index].dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) 
        {
            // audio
            ret = avcodec_send_packet(stream_ctx[stream_index].dec_ctx, packet);
        }
        if (ret < 0) 
        {
            av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
            break;
        }

        while (ret >= 0) 
        {
            ret = avcodec_receive_frame(stream_ctx[stream_index].dec_ctx, stream_ctx[stream_index].dec_frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                break;
            else if (ret < 0)
                goto end;

            ret = encode_write_frame(stream_index, 0);
            if (ret < 0)
                goto end;
        }

        av_packet_unref(packet);
    }

    // flush encoders
    for (i = 0; i < ifmt_ctx->nb_streams; i++) 
    {
        if (stream_ctx[i].enc_ctx) 
        {
            ret = flush_encoder(i);
            if (ret < 0)
                goto end;
        }
    }


    av_write_trailer(ofmt_ctx);
end:
    av_packet_free(&packet);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) 
    {
        avcodec_free_context(&stream_ctx[i].dec_ctx);
        if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
            avcodec_free_context(&stream_ctx[i].enc_ctx);

        av_frame_free(&stream_ctx[i].dec_frame);
    }
    av_free(stream_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}