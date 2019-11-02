/*
 * HVQM4 Video Decoder
 * Copyright (c) 2019 Tillmann Karras
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "internal.h"
#include "libavutil/intreadwrite.h"

#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"

// FIXME
#define NATIVE
#define HVQM4_NOMAIN
#include "h4m_audio_decode.c"

typedef struct
{
    // TODO: inline Player and SeqObj
    Player player;
    AVFrame *past;
    AVFrame *future;
} Hvqm4DecodeContext;

static av_cold int hvqm4_init(AVCodecContext *ctx)
{
    av_log(ctx, AV_LOG_DEBUG, "hvqm4_init\n");
    Hvqm4DecodeContext *h4m = ctx->priv_data;
    h4m->past = av_frame_alloc();
    h4m->future = av_frame_alloc();
    Player *player = &h4m->player;
    SeqObj *seqobj = &player->seqobj;

    HVQM4InitDecoder();
    seqobj->width = ctx->width;
    seqobj->height = ctx->height;
    if (ctx->extradata_size < 2)
        return AVERROR_INVALIDDATA;
    seqobj->h_samp = ctx->extradata[0];
    seqobj->v_samp = ctx->extradata[1];
    VideoState *state = malloc(HVQM4BuffSize(seqobj));
    HVQM4SetBuffer(seqobj, state);
    decv_init(player);

    if (seqobj->h_samp == 2 && seqobj->v_samp == 2)
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    else {
        av_log(ctx, AV_LOG_ERROR, "pixel format not implemented: h_samp:%u v_samp:%u\n", seqobj->h_samp, seqobj->v_samp);
        return AVERROR_PATCHWELCOME;
    }
    ctx->color_range = AVCOL_RANGE_JPEG; // just a guess

    return 0;
}

static av_cold int hvqm4_close(AVCodecContext *ctx)
{
    return 0;
}

static int hvqm4_flush(AVCodecContext *ctx)
{
    return 0;
}

enum Hvqm4FrameType
{
    HVQM4_I_FRAME = 0x10,
    HVQM4_P_FRAME = 0x20,
    HVQM4_B_FRAME = 0x30,
};

static int hvqm4_decode(AVCodecContext *ctx, void *data, int *got_frame, AVPacket *pkt)
{
    //av_log(ctx, AV_LOG_DEBUG, "hvqm4_decode\n");
    Hvqm4DecodeContext *h4m = ctx->priv_data;
    AVFrame *frame = data;
    Player *player = &h4m->player;
    SeqObj *seqobj = &player->seqobj;
    int ret;

    //TODO: AV_GET_BUFFER_FLAG_REF
    if ((ret = ff_reget_buffer(ctx, frame, 0)) < 0)
        return ret;

    uint16_t frame_type = AV_RB16(pkt->data);
    // FIXME: pts is GOP relative but should be global
    pkt->pts = AV_RB32(pkt->data + 2);

    // swap past and future
    if (frame_type != HVQM4_B_FRAME) {
        AVFrame *tmp = h4m->past;
        h4m->past = h4m->future;
        h4m->future = tmp;
    }

    switch (frame_type)
    {
        case HVQM4_I_FRAME:
            av_log(ctx, AV_LOG_DEBUG, "I frame\n");
            HVQM4DecodeIpic(seqobj, pkt->data + 6, frame->data);
            frame->pict_type = AV_PICTURE_TYPE_I;
            break;
        case HVQM4_P_FRAME:
            av_log(ctx, AV_LOG_DEBUG, "P frame\n");
            HVQM4DecodePpic(seqobj, pkt->data + 6, frame->data, h4m->past->data);
            frame->pict_type = AV_PICTURE_TYPE_P;
            break;
        case HVQM4_B_FRAME:
            av_log(ctx, AV_LOG_DEBUG, "B frame\n");
            HVQM4DecodeBpic(seqobj, pkt->data + 6, frame->data, h4m->past->data, h4m->future->data);
            frame->pict_type = AV_PICTURE_TYPE_B;
            break;
        default:
            av_log(ctx, AV_LOG_ERROR, "unknown frame type\n");
            return AVERROR_INVALIDDATA;
    }
    frame->key_frame = frame->pict_type == AV_PICTURE_TYPE_I;

    // swap present and future
    if (frame_type != HVQM4_B_FRAME) {
        av_frame_unref(h4m->future);
        av_frame_ref(h4m->future, frame);
    }

    *got_frame = 1;
    return pkt->size;
}

AVCodec ff_hvqm4_decoder = {
    .name = "hvqm4",
    .long_name = NULL_IF_CONFIG_SMALL("Hudson HVQM4 video"),
    .type = AVMEDIA_TYPE_VIDEO,
    .id = AV_CODEC_ID_HVQM4,
    .capabilities = 0,
    .priv_data_size = sizeof(Hvqm4DecodeContext),
    .init = hvqm4_init,
    .close = hvqm4_close,
    .decode = hvqm4_decode,
    // FIXME: this is supposedly used for seeking
    //.flush = hvqm4_flush,
    .caps_internal = 0,
};
