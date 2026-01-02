#pragma once

#include "types.h"

#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>

int sub_to_frame_convert(SubToFrameContext *stf_ctx, InputContext *in_ctx);
void sub_to_frame_context_free(SubToFrameContext **stf_ctx);
BurnInFilterContext *burn_in_filter_context_init(ProcessingContext *proc_ctx,
  InputContext *in_ctx);
void burn_in_filter_context_free(BurnInFilterContext **burn_in_ctx);
