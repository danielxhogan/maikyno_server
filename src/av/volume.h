#include "types.h"

VolumeFilterContext *volume_filter_context_init(
  StreamConfig *stream_cfg, AVCodecContext *enc_ctx, int rendition);
void volume_filter_context_free(VolumeFilterContext **vol_ctx);