#pragma once

#include "../utils/common.h"

typedef struct SwrOutputContext {
  struct SwrContext *swr_ctx;
  AVFrame *swr_frame;
  int nb_converted_samples;
} SwrOutputContext;

SwrOutputContext *swr_output_context_alloc(AVCodecContext *dec_ctx,
  AVCodecContext *enc_ctx);
void swr_output_context_free(SwrOutputContext **swr_out_ctx);
