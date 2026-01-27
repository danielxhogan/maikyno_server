#pragma once

#include "types.h"

RenditionFilterContext *rendition_filter_context_init(
  ProcessingContext *proc_ctx, StreamContext *stream_ctx);
void rendition_filter_context_free(RenditionFilterContext **rend_ctx);