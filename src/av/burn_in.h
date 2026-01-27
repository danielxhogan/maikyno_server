#pragma once

#include "types.h"

#include <libswscale/swscale.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>

#define SUBTITLE_OUTPUT_PIX_FMT AV_PIX_FMT_RGBA

int sub_to_frame_convert(ProcessingContext *proc_ctx);
int push_dummy_subtitle(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, int64_t pts);
void sub_to_frame_context_free(SubToFrameContext **stf_ctx);
BurnInFilterContext *burn_in_filter_context_init(ProcessingContext *proc_ctx);
void burn_in_filter_context_free(BurnInFilterContext **burn_in_ctx);
