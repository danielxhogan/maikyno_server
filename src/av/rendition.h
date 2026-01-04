#pragma once

#include "types.h"

#include <libavutil/pixdesc.h>

RenditionFilterContext *video_rendition_filter_context_init(
  ProcessingContext *proc_ctx, AVCodecContext *dec_ctx,
  AVCodecContext *enc_ctx1, AVCodecContext *enc_ctx2, AVStream *in_stream);
void rendition_filter_context_free(RenditionFilterContext **rend_ctx);