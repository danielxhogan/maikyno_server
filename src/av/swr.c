#include "swr.h"

SwrOutputContext *swr_output_context_alloc(AVCodecContext *dec_ctx,
  AVCodecContext *enc_ctx)
{
  int ret = 0;
  SwrOutputContext *swr_out_ctx;

  if (!(swr_out_ctx = malloc(sizeof(SwrOutputContext)))) {
    fprintf(stderr, "Failed to allocate swr output context.\n");
    return NULL;
  }

  swr_out_ctx->swr_ctx = NULL;
  swr_out_ctx->swr_frame = NULL;
  swr_out_ctx->nb_converted_samples = 0;

  if (!(swr_out_ctx->swr_ctx = swr_alloc())) {
    fprintf(stderr, "Failed to allocate SwrContext.\n");
    ret = -ENOMEM;
    goto end;
  }

  av_opt_set_chlayout(swr_out_ctx->swr_ctx,
    "in_chlayout", &dec_ctx->ch_layout, 0);
  av_opt_set_int(swr_out_ctx->swr_ctx,
    "in_sample_rate", dec_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(swr_out_ctx->swr_ctx,
    "in_sample_fmt", dec_ctx->sample_fmt, 0);
  av_opt_set_chlayout(swr_out_ctx->swr_ctx,
    "out_chlayout", &enc_ctx->ch_layout, 0);
  av_opt_set_int(swr_out_ctx->swr_ctx,
    "out_sample_rate", enc_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(swr_out_ctx->swr_ctx,
    "out_sample_fmt", enc_ctx->sample_fmt, 0);

  if ((ret = swr_init(swr_out_ctx->swr_ctx)) < 0) {
    fprintf(stderr, "Failed to initialize SwrContext.\n\
      Libav Error: %s\n", av_err2str(ret));
    goto end;
  }

  if (!(swr_out_ctx->swr_frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = -ENOMEM;
    goto end;
  }

  swr_out_ctx->swr_frame->format = enc_ctx->sample_fmt;
  av_channel_layout_copy(&swr_out_ctx->swr_frame->ch_layout,
    &enc_ctx->ch_layout);
  swr_out_ctx->swr_frame->sample_rate = enc_ctx->sample_rate;
  swr_out_ctx->swr_frame->nb_samples = 1536;

  if ((ret = av_frame_get_buffer(swr_out_ctx->swr_frame, 0)) < 0) {
    fprintf(stderr, "Failed to allocate buffers for frame.\n\
      Libav Error: %s\n", av_err2str(ret));
    goto end;
  }

end:
  if (ret < 0) {
    swr_output_context_free(&swr_out_ctx);
    return NULL;
  }

  return swr_out_ctx;
}

void swr_output_context_free(SwrOutputContext **swr_out_ctx)
{
  if (!*swr_out_ctx) return;
  swr_free(&(*swr_out_ctx)->swr_ctx);
  av_frame_free(&(*swr_out_ctx)->swr_frame);
  free(*swr_out_ctx);
  *swr_out_ctx = NULL;
}
