/*
 * HVQM4 Demuxer
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

#include "avformat.h"
#include "avio.h"
#include "internal.h"

// FIXME: enable these warnings
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"
#pragma GCC diagnostic ignored "-Wunused-variable"

static int hvqm4_read_probe(AVProbeData *p)
{
    const char magic13[16] = "HVQM4 1.3";
    const char magic15[16] = "HVQM4 1.5";
    if (memcmp(p->buf, magic13, 16) == 0)
        return AVPROBE_SCORE_MAX;
    if (memcmp(p->buf, magic15, 16) == 0)
        return AVPROBE_SCORE_MAX;
    return 0;
}

typedef struct
{
    int video_stream_index;
    int audio_stream_index;
} Hvqm4DemuxContext;

static int hvqm4_read_header(AVFormatContext *ctx)
{
    Hvqm4DemuxContext *h4m = ctx->priv_data;
    AVIOContext *pb = ctx->pb;

    // skip signature
    avio_seek(pb, 0x10, SEEK_SET);

    uint32_t header_size = avio_rb32(pb);
    if (header_size != 0x44)
        return AVERROR_INVALIDDATA;
    uint32_t body_size = avio_rb32(pb);
    uint32_t gop_count = avio_rb32(pb);
    uint32_t video_frames = avio_rb32(pb);
    uint32_t audio_frames = avio_rb32(pb);
    uint32_t frame_usec = avio_rb32(pb);
    uint32_t max_frame_size = avio_rb32(pb);
    avio_rb32(pb); // unknown
    uint32_t audio_frame_size = avio_rb32(pb);
    uint16_t width = avio_rb16(pb);
    uint16_t height = avio_rb16(pb);
    uint8_t hsamp = avio_r8(pb);
    uint8_t vsamp = avio_r8(pb);
    uint8_t video_mode = avio_r8(pb);
    uint8_t unk3B = avio_r8(pb);
    uint8_t audio_channels = avio_r8(pb);
    uint8_t audio_bitdepth = avio_r8(pb);
    avio_rb16(pb); // probably padding
    uint32_t audio_sample_rate = avio_rb32(pb);

    if (audio_frames) {
        AVStream *aud = avformat_new_stream(ctx, NULL);
        if (!aud)
            return AVERROR(ENOMEM);
        // pts is in microseconds
        avpriv_set_pts_info(aud, 64, 1, 1000000);
        aud->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        aud->codecpar->codec_id = AV_CODEC_ID_NONE;
        h4m->audio_stream_index = aud->index;
    }

    if (video_frames) {
        AVStream *vid = avformat_new_stream(ctx, NULL);
        if (!vid)
            return AVERROR(ENOMEM);
        avpriv_set_pts_info(vid, 64, frame_usec, 1000000);
        vid->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        vid->codecpar->codec_id = AV_CODEC_ID_HVQM4;
        vid->codecpar->codec_tag = 0; // no FOURCC
        // FIXME: this should depend on hsamp and vsamp
        vid->codecpar->format = AV_PIX_FMT_YUV420P;
        // FIXME: output samples need to be shifted!
        vid->codecpar->bits_per_raw_sample = 8;
        vid->codecpar->width = width;
        vid->codecpar->height = height;
        vid->codecpar->color_range = AVCOL_RANGE_JPEG; // ?
        vid->codecpar->chroma_location = AVCHROMA_LOC_CENTER; // ?
        vid->nb_frames = video_frames;
        vid->duration = video_frames;
        h4m->video_stream_index = vid->index;
    }

    return 0;
}

static int hvqm4_read_packet(AVFormatContext *ctx, AVPacket *pkt)
{
    av_log(ctx, AV_LOG_DEBUG, "hvqm4_read_packet\n");
    Hvqm4DemuxContext *h4m = ctx->priv_data;
    AVIOContext *pb = ctx->pb;
    // TODO: implement seeking
    uint32_t prev_gop_offset = avio_rb32(pb);
    uint32_t next_gop_offset = avio_rb32(pb);
    uint32_t gop_video_frames = avio_rb32(pb);
    uint32_t gop_audio_frames = avio_rb32(pb);
    avio_rb32(pb); // unknown
    uint16_t media_type = avio_rb16(pb);
    uint16_t frame_type = avio_rb16(pb);
    uint32_t frame_size = avio_rb32(pb);

    av_get_packet(pb, pkt, frame_size);
    if (media_type == 0) {
        av_log(ctx, AV_LOG_DEBUG, "audio packet\n");
        pkt->stream_index = h4m->audio_stream_index;
    } else if (media_type == 1) {
        av_log(ctx, AV_LOG_DEBUG, "video packet\n");
        pkt->stream_index = h4m->video_stream_index;
        if (frame_type == 10)
            pkt->flags |= AV_PKT_FLAG_KEY;
        if (frame_type == 30)
            pkt->flags |= AV_PKT_FLAG_DISPOSABLE;
    } else {
        av_log(ctx, AV_LOG_ERROR, "unknown media type\n");
    }
    return 0;
}

static int hvqm4_read_seek(AVFormatContext *ctx, int stream_index, int64_t timestamp, int flags)
{
    Hvqm4DemuxContext *h4m = ctx->priv_data;
    AVStream *st = ctx->streams[stream_index];

    // TODO
    return -1;
}

AVInputFormat ff_hvqm4_demuxer = {
    .name           = "hvqm4",
    .long_name      = NULL_IF_CONFIG_SMALL("Hudson HVQM4"),
    .extensions     = "h4m",
    .priv_data_size = sizeof(Hvqm4DemuxContext),
    .read_probe     = hvqm4_read_probe,
    .read_header    = hvqm4_read_header,
    .read_packet    = hvqm4_read_packet,
    .read_seek      = hvqm4_read_seek,
    .flags          = AVFMT_SHOW_IDS,
};
