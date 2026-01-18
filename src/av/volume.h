#include "types.h"

#include <libavutil/pixdesc.h>

VolumeFilterContext *volume_filter_context_init(ProcessingContext *proc_ctx,
  OutputContext *out_ctx, int ctx_idx, int out_stream_idx, int rendition2);
void volume_filter_context_free(VolumeFilterContext **vol_ctx);