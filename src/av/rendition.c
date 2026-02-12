#include "rendition.h"

#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>

int buffersink_ctx_init(AVFilterContext **buffersink_ctx,
  AVFilterGraph *filter_graph, const char *pix_fmt)
{
  int ret = 0;
  const AVFilter *buffersink = avfilter_get_by_name("buffersink");

  if (!(*buffersink_ctx =
    avfilter_graph_alloc_filter(filter_graph, buffersink, "out")))
  {
    fprintf(stderr, "Failed to create buffer sink.\n");
    ret = AVERROR(ENOMEM);
    return ret;
  }

  if ((ret = av_opt_set(*buffersink_ctx, "pixel_formats",
    pix_fmt, AV_OPT_SEARCH_CHILDREN)))
  {
    fprintf(stderr, "Failed to set pixel format on buffersink.\n");
    return ret;
  }

  if ((ret = avfilter_init_dict(*buffersink_ctx, NULL))) {
    fprintf(stderr, "Failed to initialize buffersink.\n");
    return ret;
  }

  return 0;
}

RenditionFilterContext *rendition_filter_context_init(
  ProcessingContext *proc_ctx, StreamContext *stream_ctx)
{
  int ret = 0;
  char args[512], flt_str[512];

  AVCodecContext *dec_ctx = stream_ctx->dec_ctx;
  AVCodecContext *enc_ctx1 = stream_ctx->rend0_enc_ctx;
  AVCodecContext *enc_ctx2 = stream_ctx->rend1_enc_ctx;
  AVStream *in_stream = stream_ctx->in_stream;

  const char *pix_fmt_str1 = av_get_pix_fmt_name(enc_ctx1->pix_fmt);
  const char *pix_fmt_str2 = av_get_pix_fmt_name(enc_ctx2->pix_fmt);

  if (proc_ctx->tonemap && proc_ctx->hdr) {
    snprintf(flt_str, sizeof(flt_str),
      "[in]split[out1][tmp];[tmp]zscale=t=linear:npl=100,format=gbrpf32le,zscale=p=bt709,tonemap=tonemap=hable,zscale=w=%d:h=%d:t=bt709:m=bt709:c=left,format=%s[out2]",
      enc_ctx2->width, enc_ctx2->height, pix_fmt_str2);
  } else {
    snprintf(flt_str, sizeof(flt_str),
      "[in]split[out1][tmp];[tmp]scale=w=%d:h=%d,format=%s[out2]",
      enc_ctx2->width, enc_ctx2->height, pix_fmt_str2);
  }

  const AVFilter *buffersrc = avfilter_get_by_name("buffer");

  AVFilterInOut *outputs = NULL;
  AVFilterInOut *inputs = NULL;
  RenditionFilterContext *v_rend_ctx = NULL;

  if (!(v_rend_ctx = malloc(sizeof(RenditionFilterContext)))) {
    fprintf(stderr, "Failed to allocate RenditionFilterContext.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  v_rend_ctx->buffersink_ctx1 = NULL;
  v_rend_ctx->buffersink_ctx2 = NULL;
  v_rend_ctx->buffersrc_ctx = NULL;
  v_rend_ctx->filter_graph = NULL;
  v_rend_ctx->filtered_frame1 = NULL;
  v_rend_ctx->filtered_frame2 = NULL;

  if (!(outputs = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate outputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(inputs = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate inputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(v_rend_ctx->filter_graph = avfilter_graph_alloc())) {
    fprintf(stderr, "Failed to allocate filter graph.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  snprintf(args, sizeof(args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
    in_stream->time_base.num, in_stream->time_base.den,
    dec_ctx->sample_aspect_ratio.num,
    dec_ctx->sample_aspect_ratio.den);

  if ((ret = avfilter_graph_create_filter(&v_rend_ctx->buffersrc_ctx, buffersrc,
    "in", args, NULL, v_rend_ctx->filter_graph)) < 0)
  {
    fprintf(stderr, "Failed to create buffer source.\n");
    goto end;
  }

  if ((ret = buffersink_ctx_init(&v_rend_ctx->buffersink_ctx1,
    v_rend_ctx->filter_graph, pix_fmt_str1)) < 0)
  {
    fprintf(stderr, "Failed to initialize first buffer sink context.\n");
    goto end;
  }

  if ((ret = buffersink_ctx_init(&v_rend_ctx->buffersink_ctx2,
    v_rend_ctx->filter_graph, pix_fmt_str2)) < 0)
  {
    fprintf(stderr, "Failed to initialize first buffer sink context.\n");
    goto end;
  }

  outputs->name = av_strdup("in");
  outputs->filter_ctx = v_rend_ctx->buffersrc_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out1");
  inputs->filter_ctx = v_rend_ctx->buffersink_ctx1;
  inputs->pad_idx = 0;
  inputs->next = avfilter_inout_alloc();

  inputs->next->name = av_strdup("out2");
  inputs->next->filter_ctx = v_rend_ctx->buffersink_ctx2;
  inputs->next->pad_idx = 0;
  inputs->next->next = NULL;

  if ((ret = avfilter_graph_parse_ptr(v_rend_ctx->filter_graph,
    flt_str, &inputs, &outputs, NULL)) < 0)
  {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if ((ret = avfilter_graph_config(v_rend_ctx->filter_graph, NULL)) < 0) {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if (!(v_rend_ctx->filtered_frame1 = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(v_rend_ctx->filtered_frame2 = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  if (ret < 0) { return NULL; }
  return v_rend_ctx;
}

void rendition_filter_context_free(RenditionFilterContext **rend_ctx)
{
  if (!*rend_ctx) return;
  avfilter_graph_free(&(*rend_ctx)->filter_graph);
  av_frame_unref((*rend_ctx)->filtered_frame1);
  av_frame_free(&(*rend_ctx)->filtered_frame1);
  av_frame_unref((*rend_ctx)->filtered_frame2);
  av_frame_free(&(*rend_ctx)->filtered_frame2);
  free(*rend_ctx);
  *rend_ctx = NULL;
}
