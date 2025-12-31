#pragma once

#include "swr.h"
#include "fsc.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

typedef struct DeinterlaceFilterContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame;
} DeinterlaceFilterContext;

typedef struct ProcessingContext {
  unsigned int nb_in_streams;
  unsigned int nb_selected_streams;
  unsigned int nb_out_streams;

  int *ctx_map;
  int *idx_map;

  char **stream_titles_arr;
  int *passthrough_arr;

  SwrOutputContext **swr_out_ctx_arr;
  FrameSizeConversionContext **fsc_ctx_arr;

  DeinterlaceFilterContext *deint_ctx;

  int burn_in_idx;
  int *gain_boost_arr;
  int *renditions_arr;
} ProcessingContext;

typedef struct InputContext {
  AVFormatContext *fmt_ctx;
  AVCodecContext **dec_ctx;
  AVPacket *init_pkt;
  AVFrame *dec_frame;
  int nb_selected_streams;
} InputContext;

typedef struct OutputContext {
  AVFormatContext *fmt_ctx;
  AVCodecContext **enc_ctx;
  AVPacket *enc_pkt;
  int nb_out_streams;
} OutputContext;
