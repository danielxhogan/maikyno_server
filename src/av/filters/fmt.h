#pragma once

#include "../utils/common.h"

#define NO_CONVERSION 1

typedef struct FormatFilterContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame;
} FormatFilterContext;

FormatFilterContext *format_filter_context_init(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, enum AVPixelFormat in_pix_fmt, int *no_conversion);
void format_filter_context_free(FormatFilterContext **fmt_ctx);