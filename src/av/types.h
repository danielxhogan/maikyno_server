#pragma once

#include "swr.h"
#include "fsc.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>

typedef struct RenditionFilterContext {
  AVFilterContext *buffersrc_ctx;
  AVFilterContext *buffersink_ctx1;
  AVFilterContext *buffersink_ctx2;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame1;
  AVFrame *filtered_frame2;
} RenditionFilterContext;

typedef struct SubToFrameContext {
  struct SwsContext *sws_ctx;
  enum AVPixelFormat in_pix_fmt;
  enum AVPixelFormat out_pix_fmt;
  long width_ratio;
  long height_ratio;
  int scale_algo;
  AVFrame *subtitle_frame;
} SubToFrameContext;

typedef struct BurnInFilterContext {
  AVFilterContext *v_buffersrc_ctx;
  AVFilterContext *s_buffersrc_ctx;
  AVFilterContext *buffersink_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame;
  SubToFrameContext *stf_ctx;
} BurnInFilterContext;

typedef struct DeinterlaceFilterContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame;
} DeinterlaceFilterContext;

typedef struct VolumeFilterContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame;
} VolumeFilterContext;

typedef struct StreamContext {
  char *codec;
  enum AVMediaType codec_type;
  int passthrough;
  int renditions;

  int in_stream_idx;
  AVStream *in_stream;
  AVCodecContext *dec_ctx;

  int transcode_rend0;
  AVCodecContext *rend0_enc_ctx;
  AVCodecContext *rend1_enc_ctx;

  char *rend0_title;
  char *rend1_title;
  int rend0_out_stream_idx;
  int rend1_out_stream_idx;
  AVStream *rend0_out_stream;
  AVStream *rend1_out_stream;

  SwrOutputContext *rend0_swr_out_ctx;
  SwrOutputContext *rend1_swr_out_ctx;

  FrameSizeConversionContext *rend0_fsc_ctx;
  FrameSizeConversionContext *rend1_fsc_ctx;

  int rend0_gain_boost;
  int rend1_gain_boost;
  VolumeFilterContext *rend0_vol_ctx;
  VolumeFilterContext *rend1_vol_ctx;
} StreamContext;

typedef struct ProcessingContext {
  unsigned int nb_out_streams;

  int64_t last_sub_pts;
  int64_t tminus1_v_pts;
  int64_t tminus2_v_pts;

  VolumeFilterContext **vol_ctx_arr;

  // *************************************
  unsigned int nb_in_streams;
  unsigned int nb_selected_streams;
  unsigned int v_stream_idx;
  int *ctx_map;

  AVFormatContext *in_fmt_ctx;
  AVFormatContext *out_fmt_ctx;

  StreamContext **stream_ctx_arr;

  int deint;
  DeinterlaceFilterContext *deint_ctx;

  int burn_in_idx;
  int first_sub;
  BurnInFilterContext *burn_in_ctx;

  int tonemap;
  int hdr;
  RenditionFilterContext *rend_ctx;

  AVPacket *pkt;
  AVPacket *pkt_cpy;
  AVFrame *frame;
  AVFrame *frame_cpy;
  AVSubtitle *sub;
} ProcessingContext;
