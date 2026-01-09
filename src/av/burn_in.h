#pragma once

#include "types.h"

#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>

#define SUBTITLE_OUTPUT_PIX_FMT AV_PIX_FMT_RGBA

int sub_to_frame_convert(SubToFrameContext *stf_ctx,
  InputContext *in_ctx, int width, int height);
int push_dummy_subtitle(ProcessingContext *proc_ctx,
  OutputContext *out_ctx, int out_stream_idx, int64_t pts);
void sub_to_frame_context_free(SubToFrameContext **stf_ctx);
BurnInFilterContext *burn_in_filter_context_init(ProcessingContext *proc_ctx,
  InputContext *in_ctx);
void burn_in_filter_context_free(BurnInFilterContext **burn_in_ctx);
