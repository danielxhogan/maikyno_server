#include "types.h"

DeinterlaceFilterContext *deint_filter_context_init(
  ProcessingContext *proc_ctx, StreamContext *stream_ctx);
void deint_filter_context_free(DeinterlaceFilterContext **deint_ctx);
