#include "volume.h"

#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>

VolumeFilterContext *volume_filter_context_init(
  StreamContext *stream_ctx, AVCodecContext *enc_ctx, int rendition)
{
  int gain_boost, ret = 0;
  char args[512], flt_str[512], ch_layout[512];
  const char *sample_fmt;

  if (!rendition) {
    gain_boost = stream_ctx->rend0_gain_boost;
  } else {
    gain_boost = stream_ctx->rend1_gain_boost;
  }

  snprintf(flt_str, sizeof(flt_str), "volume=%ddB", gain_boost);

  const AVFilter *buffersrc = avfilter_get_by_name("abuffer");
  const AVFilter *buffersink = avfilter_get_by_name("abuffersink");

  VolumeFilterContext *vol_ctx = NULL;
  AVFilterInOut *outputs = NULL;
  AVFilterInOut *inputs = NULL;

  if (!(vol_ctx = malloc(sizeof(VolumeFilterContext)))) {
    fprintf(stderr, "Failed to allocate VolumeFilterContext.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  vol_ctx->buffersink_ctx = NULL;
  vol_ctx->buffersrc_ctx = NULL;
  vol_ctx->filter_graph = NULL;
  vol_ctx->frame = NULL;

  if (!(outputs = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate outputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(inputs = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate outputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(vol_ctx->filter_graph = avfilter_graph_alloc())) {
    fprintf(stderr, "Failed to allocate filter graph.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  sample_fmt = av_get_sample_fmt_name(enc_ctx->sample_fmt);

  ret = snprintf(args, sizeof(args),
    "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=",
      enc_ctx->sample_rate, enc_ctx->sample_rate, sample_fmt);

    av_channel_layout_describe(&enc_ctx->ch_layout, args + ret, sizeof(args) - ret);

  if ((ret = avfilter_graph_create_filter(&vol_ctx->buffersrc_ctx, buffersrc,
    "in", args, NULL, vol_ctx->filter_graph)) < 0)
  {
    fprintf(stderr, "Failed to create buffer source.\n");
    goto end;
  }

  if (!(vol_ctx->buffersink_ctx =
    avfilter_graph_alloc_filter(vol_ctx->filter_graph, buffersink, "out")))
  {
    fprintf(stderr, "Failed to create buffer sink.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if ((ret = av_opt_set(vol_ctx->buffersink_ctx, "sample_formats",
    sample_fmt, AV_OPT_SEARCH_CHILDREN)))
  {
    fprintf(stderr, "Failed to set sample format on buffersink.\n");
    goto end;
  }

  av_channel_layout_describe(&enc_ctx->ch_layout, ch_layout, sizeof(ch_layout));

  if ((ret = av_opt_set(vol_ctx->buffersink_ctx, "channel_layouts",
    ch_layout, AV_OPT_SEARCH_CHILDREN)))
  {
    fprintf(stderr, "Failed to set channel layout on buffersink.\n");
    goto end;
  }
  
  if ((ret = av_opt_set_array(vol_ctx->buffersink_ctx, "samplerates",
    AV_OPT_SEARCH_CHILDREN, 0, 1, AV_OPT_TYPE_INT, &enc_ctx->sample_rate)))
  {
    fprintf(stderr, "Failed to set channel layout on buffersink.\n");
    goto end;
  }

  if ((ret = avfilter_init_dict(vol_ctx->buffersink_ctx, NULL))) {
    fprintf(stderr, "Failed to initialize buffersink.\n");
    goto end;
  }

  outputs->name = av_strdup("in");
  outputs->filter_ctx = vol_ctx->buffersrc_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = vol_ctx->buffersink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  if ((ret = avfilter_graph_parse_ptr(vol_ctx->filter_graph,
    flt_str, &inputs, &outputs, NULL)) < 0)
  {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if ((ret = avfilter_graph_config(vol_ctx->filter_graph, NULL)) < 0) {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if (!(vol_ctx->frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  if (ret < 0) {
    volume_filter_context_free(&vol_ctx);
    return NULL;
  }
  return vol_ctx;
}

void volume_filter_context_free(VolumeFilterContext **vol_ctx)
{
  if (!*vol_ctx) return;
  avfilter_graph_free(&(*vol_ctx)->filter_graph);
  av_frame_unref((*vol_ctx)->frame);
  av_frame_free(&(*vol_ctx)->frame);
  free(*vol_ctx);
  *vol_ctx = NULL;
}