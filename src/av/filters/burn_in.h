#pragma once

#include "../utils/common.h"

#define SUBTITLE_OUTPUT_PIX_FMT AV_PIX_FMT_RGBA

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

int sub_to_frame_convert(ProcessingContext *proc_ctx);
int push_dummy_subtitle(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, int64_t pts);
void sub_to_frame_context_free(SubToFrameContext **stf_ctx);
BurnInFilterContext *burn_in_filter_context_init(ProcessingContext *proc_ctx);
void burn_in_filter_context_free(BurnInFilterContext **burn_in_ctx);
