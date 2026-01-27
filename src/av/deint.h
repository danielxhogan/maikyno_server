#include "types.h"

DeinterlaceFilterContext *deint_filter_context_init(AVCodecContext *dec_ctx,
  InputContext *in_ctx, int in_stream_idx);
void deint_filter_context_free(DeinterlaceFilterContext **deint_ctx);
