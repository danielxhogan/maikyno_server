#pragma once

#include "../utils/common.h"

typedef struct VolumeFilterContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;
  AVFrame *frame;
} VolumeFilterContext;

VolumeFilterContext *volume_filter_context_init(
  StreamContext *stream_ctx, AVCodecContext *enc_ctx, int rendition);
void volume_filter_context_free(VolumeFilterContext **vol_ctx);