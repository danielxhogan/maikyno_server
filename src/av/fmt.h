#include "types.h"

#define NO_CONVERSION 1

FormatFilterContext *format_filter_context_init(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, enum AVPixelFormat in_pix_fmt, int *no_conversion);
void format_filter_context_free(FormatFilterContext **fmt_ctx);