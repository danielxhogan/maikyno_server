#pragma once

#include "common.h"

typedef struct RenditionFilterContext {
  AVFilterContext *buffersrc_ctx;
  AVFilterContext *buffersink_ctx1;
  AVFilterContext *buffersink_ctx2;
  AVFilterGraph *filter_graph;
  AVFrame *filtered_frame1;
  AVFrame *filtered_frame2;
} RenditionFilterContext;

RenditionFilterContext *rendition_filter_context_init(
  ProcessingContext *proc_ctx, StreamContext *stream_ctx);
void rendition_filter_context_free(RenditionFilterContext **rend_ctx);