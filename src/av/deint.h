#pragma once

#include "common.h"

typedef struct DeinterlaceFilterContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame;
} DeinterlaceFilterContext;

DeinterlaceFilterContext *deint_filter_context_init(
  ProcessingContext *proc_ctx, StreamContext *stream_ctx);
void deint_filter_context_free(DeinterlaceFilterContext **deint_ctx);
