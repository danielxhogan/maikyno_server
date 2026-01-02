#pragma once

#include "types.h"

RenditionFilterContext *video_rendition_filter_context_init(
  AVCodecContext *dec_ctx, AVStream *in_stream);
void rendition_filter_context_free(RenditionFilterContext **rend_ctx);