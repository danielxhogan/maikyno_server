#include "types.h"

VolumeFilterContext *volume_filter_context_init(
  StreamContext *stream_ctx, StreamConfig *stream_cfg,
  AVCodecContext *enc_ctx, int rendition);
void volume_filter_context_free(VolumeFilterContext **vol_ctx);