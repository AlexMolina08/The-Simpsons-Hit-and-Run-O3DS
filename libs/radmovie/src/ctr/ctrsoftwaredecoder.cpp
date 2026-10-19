#include "ctrsoftwaredecoder.hpp"
#include <string.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
}

struct SoftwareMovieDecoder
{
    AVCodecContext* codec;
    AVCodecParserContext* parser;
    AVPacket* packet;
    AVFrame* frame;
    unsigned char* input;
    int remaining, cursor;
    unsigned width, height;
    bool parserFlushed, decoderFlushed;
};

static const unsigned INPUT_BYTES = 32 * 1024;

void DestroySoftwareMovieDecoder(SoftwareMovieDecoder* decoder)
{
    if(!decoder) return;
    av_frame_free(&decoder->frame);
    av_packet_free(&decoder->packet);
    avcodec_free_context(&decoder->codec);
    if(decoder->parser) av_parser_close(decoder->parser);
    av_free(decoder->input);
    av_free(decoder);
}

SoftwareMovieDecoder* CreateSoftwareMovieDecoder(unsigned width, unsigned height)
{
    SoftwareMovieDecoder* decoder = (SoftwareMovieDecoder*)av_mallocz(sizeof(SoftwareMovieDecoder));
    if(!decoder) return NULL;
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    decoder->codec = avcodec_alloc_context3(codec);
    decoder->parser = av_parser_init(AV_CODEC_ID_MPEG1VIDEO);
    decoder->packet = av_packet_alloc();
    decoder->frame = av_frame_alloc();
    decoder->input = (unsigned char*)av_mallocz(INPUT_BYTES + AV_INPUT_BUFFER_PADDING_SIZE);
    decoder->width = width;
    decoder->height = height;
    if(!codec || !decoder->codec || !decoder->parser || !decoder->packet || !decoder->frame || !decoder->input)
    {
        DestroySoftwareMovieDecoder(decoder);
        return NULL;
    }
    decoder->codec->thread_count = 1;
    decoder->codec->thread_type = 0;
    decoder->codec->flags |= AV_CODEC_FLAG_LOW_DELAY;
    if(avcodec_open2(decoder->codec, codec, NULL) < 0)
    {
        DestroySoftwareMovieDecoder(decoder);
        return NULL;
    }
    return decoder;
}

bool DecodeSoftwareMovieFrame(SoftwareMovieDecoder* decoder, FILE* file,
                             uint32_t end, uint32_t* position, SoftwareMovieFrame* output)
{
    if(!decoder || !file || !position || !output) return false;
    for(;;)
    {
        const int received = avcodec_receive_frame(decoder->codec, decoder->frame);
        if(received == 0)
        {
            const AVFrame* frame = decoder->frame;
            if(frame->format != AV_PIX_FMT_YUV420P || frame->width != (int)decoder->width ||
               frame->height != (int)decoder->height || frame->linesize[0] <= 0 ||
               frame->linesize[1] <= 0 || frame->linesize[1] != frame->linesize[2]) return false;
            output->y = frame->data[0];
            output->cb = frame->data[1];
            output->cr = frame->data[2];
            output->width = frame->width;
            output->height = frame->height;
            output->yPitch = frame->linesize[0];
            output->chromaPitch = frame->linesize[1];
            return true;
        }
        if(received != AVERROR(EAGAIN)) return false;
        if(!decoder->remaining && *position < end)
        {
            const unsigned wanted = end - *position < INPUT_BYTES ? end - *position : INPUT_BYTES;
            const size_t got = fread(decoder->input, 1, wanted, file);
            if(!got) return false;
            *position += got;
            decoder->remaining = got;
            decoder->cursor = 0;
            // Optimized bit readers may read beyond the packet's logical end.
            memset(decoder->input + got, 0, AV_INPUT_BUFFER_PADDING_SIZE);
        }
        av_packet_unref(decoder->packet);
        if(decoder->remaining)
        {
            const int used = av_parser_parse2(decoder->parser, decoder->codec,
                &decoder->packet->data, &decoder->packet->size,
                decoder->input + decoder->cursor, decoder->remaining,
                AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if(used < 0 || (!used && !decoder->packet->size)) return false;
            decoder->cursor += used;
            decoder->remaining -= used;
        }
        else if(!decoder->parserFlushed)
        {
            if(av_parser_parse2(decoder->parser, decoder->codec,
                &decoder->packet->data, &decoder->packet->size, NULL, 0,
                AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0) < 0) return false;
            decoder->parserFlushed = true;
        }
        if(decoder->packet->size)
        {
            if(avcodec_send_packet(decoder->codec, decoder->packet) < 0) return false;
        }
        else if(decoder->parserFlushed)
        {
            if(decoder->decoderFlushed) return false;
            if(avcodec_send_packet(decoder->codec, NULL) < 0) return false;
            decoder->decoderFlushed = true;
        }
    }
}
