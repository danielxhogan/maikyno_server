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

typedef struct StreamConfig {
  char *codec;

  char *rend1_title;
  char *rend2_title;

  int passthrough;
  int renditions;

  int tonemap;
  int deinterlace;

  int rend1_gain_boost;
  int rend2_gain_boost;

  int burn_in;
} StreamConfig;

typedef struct StreamProcessingContext {
  enum AVMediaType *codec_type;
  AVStream *in_stream;
  int *in_stream_idx;
  int rend1_out_stream_idx;
  int rend2_out_stream_idx;

  AVCodecContext *dec_ctx;
  AVPacket *init_pkt;
  AVPacket *init_pkt_cpy;
  AVFrame *dec_frame;
  AVFrame *dec_frame_cpy;
  AVSubtitle *dec_sub;

  DeinterlaceFilterContext *deint_ctx;
  int hdr;
  RenditionFilterContext *rend_ctx;

  SwrOutputContext *rend1_swr_out_ctx;
  SwrOutputContext *rend2_swr_out_ctx;
  FrameSizeConversionContext *rend1_fsc_ctx;
  FrameSizeConversionContext *rend2_fsc_ctx;
  VolumeFilterContext *rend1_vol_ctx;
  VolumeFilterContext *rend2_vol_ctx;

  BurnInFilterContext *burn_in_ctx;

  AVCodecContext rend1_enc_ctx;
  AVCodecContext rend2_enc_ctx;

} StreamProcessingContext;

typedef struct ProcessingContext {
  unsigned int nb_in_streams;
  unsigned int nb_selected_streams;
  unsigned int nb_out_streams;

  unsigned int v_stream_idx;

  int *ctx_map;
  int *idx_map;

  char **stream_titles_arr;
  char **stream_rend_titles_arr;
  int *passthrough_arr;

  SwrOutputContext **swr_out_ctx_arr;
  FrameSizeConversionContext **fsc_ctx_arr;

  int deint;
  DeinterlaceFilterContext *deint_ctx;

  int burn_in_idx;
  int first_sub;
  int64_t last_sub_pts;
  int64_t tminus1_v_pts;
  int64_t tminus2_v_pts;
  BurnInFilterContext *burn_in_ctx;

  int *renditions_arr;
  int tonemap;
  int hdr;
  char **codecs;
  RenditionFilterContext **rend_ctx_arr;

  int *gain_boost_arr;
  int *gain_boost2_arr;
  VolumeFilterContext **vol_ctx_arr;

  AVFormatContext *in_fmt_ctx;
  AVFormatContext *out_fmt_ctx;
  StreamConfig **stream_cfg_arr;
  StreamProcessingContext **stream_proc_ctx_arr;
} ProcessingContext;

typedef struct OutputContext {
  AVFormatContext *fmt_ctx;
  AVCodecContext **enc_ctx_arr;
  AVPacket *enc_pkt;
  int nb_out_streams;
} OutputContext;

typedef struct InputContext {
  AVFormatContext *fmt_ctx;
  AVCodecContext **dec_ctx;
  AVPacket *init_pkt;
  AVPacket *init_pkt_cpy;
  AVFrame *dec_frame;
  AVFrame *dec_frame_cpy;
  AVSubtitle *dec_sub;
  int nb_selected_streams;
} InputContext;
