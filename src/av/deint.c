#include "deint.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>

DeinterlaceFilterContext *deint_filter_context_init(
  ProcessingContext *proc_ctx, StreamContext *stream_ctx)
{
  int ret = 0;
  char args[512], *flt_str = "yadif";
  const char *pix_fmt;

  const AVFilter *buffersrc = avfilter_get_by_name("buffer");
  const AVFilter *buffersink = avfilter_get_by_name("buffersink");
  AVCodecContext *dec_ctx = stream_ctx->dec_ctx;
  AVStream *in_stream = stream_ctx->in_stream;

  DeinterlaceFilterContext *deint_ctx = NULL;
  AVFilterInOut *outputs = NULL;
  AVFilterInOut *inputs = NULL;

  if (!(deint_ctx = malloc(sizeof(DeinterlaceFilterContext)))) {
    fprintf(stderr, "Failed to allocate DeinterlaceFilterContext.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  deint_ctx->buffersink_ctx = NULL;
  deint_ctx->buffersrc_ctx = NULL;
  deint_ctx->filter_graph = NULL;
  deint_ctx->filtered_frame = NULL;

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

  if (!(deint_ctx->filter_graph = avfilter_graph_alloc())) {
    fprintf(stderr, "Failed to allocate filter graph.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  snprintf(args, sizeof(args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    dec_ctx->width, dec_ctx->height, proc_ctx->formatted_pix_fmt,
    in_stream->time_base.num, in_stream->time_base.den,
    dec_ctx->sample_aspect_ratio.num,
    dec_ctx->sample_aspect_ratio.den);

  if ((ret = avfilter_graph_create_filter(&deint_ctx->buffersrc_ctx, buffersrc,
    "in", args, NULL, deint_ctx->filter_graph)) < 0)
  {
    fprintf(stderr, "Failed to create buffer source.\n");
    goto end;
  }

  if (!(deint_ctx->buffersink_ctx =
    avfilter_graph_alloc_filter(deint_ctx->filter_graph, buffersink, "out")))
  {
    fprintf(stderr, "Failed to create buffer sink.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  pix_fmt = av_get_pix_fmt_name(proc_ctx->formatted_pix_fmt);

  if ((ret = av_opt_set(deint_ctx->buffersink_ctx, "pixel_formats",
    pix_fmt, AV_OPT_SEARCH_CHILDREN)))
  {
    fprintf(stderr, "Failed to set pixel format on buffersink.\n");
    goto end;
  }

  if ((ret = avfilter_init_dict(deint_ctx->buffersink_ctx, NULL))) {
    fprintf(stderr, "Failed to initialize buffersink.\n");
    goto end;
  }

  outputs->name = av_strdup("in");
  outputs->filter_ctx = deint_ctx->buffersrc_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = deint_ctx->buffersink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  if ((ret = avfilter_graph_parse_ptr(deint_ctx->filter_graph,
    flt_str, &inputs, &outputs, NULL)) < 0)
  {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if ((ret = avfilter_graph_config(deint_ctx->filter_graph, NULL)) < 0) {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if (!(deint_ctx->filtered_frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  if (ret < 0) {
    deint_filter_context_free(&proc_ctx->deint_ctx);
    return NULL;
  }
  return deint_ctx;
}

void deint_filter_context_free(DeinterlaceFilterContext **deint_ctx)
{
  if (!*deint_ctx) return;
  avfilter_graph_free(&(*deint_ctx)->filter_graph);
  av_frame_unref((*deint_ctx)->filtered_frame);
  av_frame_free(&(*deint_ctx)->filtered_frame);
  free(*deint_ctx);
  *deint_ctx = NULL;
}
