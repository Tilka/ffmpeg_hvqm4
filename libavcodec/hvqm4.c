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

#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"

// FIXME
#define NATIVE
#define HVQM4_NOMAIN
#include "h4m_audio_decode.c"

typedef struct
{
    Player player;
} Hvqm4DecodeContext;

static av_cold int hvqm4_init(AVCodecContext *ctx)
{
    av_log(ctx, AV_LOG_DEBUG, "hvqm4_init\n");
    Hvqm4DecodeContext *h4m = ctx->priv_data;
    Player *player = &h4m->player;
    SeqObj *seqobj = &player->seqobj;

    HVQM4InitDecoder();
    // HACK
    seqobj->width = 640;
    seqobj->height = 480;
    seqobj->h_samp = 2;
    seqobj->v_samp = 2;
    VideoState *state = malloc(HVQM4BuffSize(seqobj));
    HVQM4SetBuffer(seqobj, state);
    decv_init(player);
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

static int hvqm4_decode(AVCodecContext *ctx, void *outdata, int *outdata_size, AVPacket *pkt)
{
    Hvqm4DecodeContext *h4m = ctx->priv_data;
    Player *player = &h4m->player;
    SeqObj *seqobj = &player->seqobj;
    if (!(pkt->flags & AV_PKT_FLAG_DISPOSABLE))
    {
        void *tmp = player->past;
        player->past = player->future;
        player->future = tmp;
    }
    if (pkt->flags & AV_PKT_FLAG_KEY)
        ;//HVQM4DecodeIpic(seqobj);
    else if (pkt->flags & AV_PKT_FLAG_DISPOSABLE)
        ;//HVQM4DecodeBpic(seqobj);
    else
        ;//HVQM4DecodePpic(seqobj);
    return 0;
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
