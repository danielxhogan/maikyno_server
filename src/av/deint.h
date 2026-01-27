#include "types.h"

DeinterlaceFilterContext *deint_filter_context_init(StreamContext *stream_ctx);
void deint_filter_context_free(DeinterlaceFilterContext **deint_ctx);
