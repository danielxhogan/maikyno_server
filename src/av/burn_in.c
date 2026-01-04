#include "burn_in.h"

SubToFrameContext *sub_to_frame_context_alloc(ProcessingContext *proc_ctx,
  InputContext *in_ctx)
{
  int ret = 0;

  enum AVPixelFormat in_pix_fmt = AV_PIX_FMT_PAL8;
  enum AVPixelFormat out_pix_fmt = AV_PIX_FMT_RGBA;

  int v_ctx_idx = proc_ctx->ctx_map[proc_ctx->v_stream_idx];
  int s_ctx_idx = proc_ctx->ctx_map[proc_ctx->burn_in_idx];

  AVCodecContext *v_dec_ctx = in_ctx->dec_ctx[v_ctx_idx];
  AVCodecContext *s_dec_ctx = in_ctx->dec_ctx[s_ctx_idx];

  SubToFrameContext *stf_ctx;

  if (!(stf_ctx = malloc(sizeof(SubToFrameContext)))) {
    fprintf(stderr, "Failed to allocate SubToFrameContext.\n");
    ret = -ENOMEM;
    goto end;
  }

  stf_ctx->sws_ctx = NULL;
  stf_ctx->in_pix_fmt = in_pix_fmt;
  stf_ctx->out_pix_fmt = out_pix_fmt;
  stf_ctx->scale_algo = SWS_BICUBIC;
  stf_ctx->subtitle_frame = NULL;

  if (!(stf_ctx->subtitle_frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate stf_ctx->subtitle_frame.\n");
    ret = -ENOMEM;
    goto end;
  }

  stf_ctx->subtitle_frame->width = v_dec_ctx->width;
  stf_ctx->subtitle_frame->height = v_dec_ctx->height;
  stf_ctx->subtitle_frame->format = out_pix_fmt;
  stf_ctx->subtitle_frame->color_primaries = v_dec_ctx->color_primaries;
  stf_ctx->subtitle_frame->color_trc = v_dec_ctx->color_trc;
  stf_ctx->subtitle_frame->colorspace = v_dec_ctx->colorspace;
  stf_ctx->subtitle_frame->chroma_location = v_dec_ctx->chroma_sample_location;
  stf_ctx->subtitle_frame->color_range = v_dec_ctx->color_range;

  if ((ret = av_frame_get_buffer(stf_ctx->subtitle_frame, 0)) < 0) {
    fprintf(stderr, "Failed to allocate buffers for frame.\n");
    goto end;
  }


  stf_ctx->width_ratio = v_dec_ctx->width / s_dec_ctx->width;
  stf_ctx->height_ratio = v_dec_ctx->height / s_dec_ctx->height;

end:
  if (ret < 0) {
    sub_to_frame_context_free(&stf_ctx);
    return NULL;
  }

  return stf_ctx;
}

int sub_to_frame_sws_context_alloc(SubToFrameContext *stf_ctx,
  AVSubtitleRect *rect)
{
  int out_width = rect->w * stf_ctx->width_ratio;
  int out_height = rect->h * stf_ctx->height_ratio;

  if (!(stf_ctx->sws_ctx = sws_getCachedContext(
    stf_ctx->sws_ctx,
    rect->w, rect->h, stf_ctx->in_pix_fmt,
    out_width, out_height, stf_ctx->out_pix_fmt,
    stf_ctx->scale_algo, NULL, NULL, NULL)))
  {
    fprintf(stderr, "Failed to get SwsContext.\n");
    return AVERROR_UNKNOWN;
  }

  return 0;
}

int sub_to_frame_convert(SubToFrameContext *stf_ctx,
  InputContext *in_ctx)
{
  int ret = 0;
  AVSubtitle *sub = in_ctx->dec_sub;
  uint8_t *dst_data[AV_NUM_DATA_POINTERS] = { NULL };

  memset(stf_ctx->subtitle_frame->data[0], 0,
    stf_ctx->subtitle_frame->height *
    stf_ctx->subtitle_frame->linesize[0]);

  for (int i = 0; i < (int) sub->num_rects; i++)
  {
    dst_data[0] = stf_ctx->subtitle_frame->data[0] +
      (sub->rects[i]->y *
        stf_ctx->subtitle_frame->linesize[0] *
        stf_ctx->height_ratio) +
      (sub->rects[i]->x * 4 * stf_ctx->width_ratio);

    if ((ret =
      sub_to_frame_sws_context_alloc(stf_ctx, sub->rects[i])) < 0)
    {
      fprintf(stderr,
        "Failed to initialize sws_context for stf_ctx.\n");
      return ret;
    }

    if ((ret = av_frame_make_writable(stf_ctx->subtitle_frame)) < 0) {
      fprintf(stderr, "Failed to make frame writable.\n");
      return ret;
    }

    ret = sws_scale(stf_ctx->sws_ctx,
      (const uint8_t * const *) sub->rects[i]->data,
      sub->rects[i]->linesize,
      0, sub->rects[i]->h,
      dst_data,
      stf_ctx->subtitle_frame->linesize
    );
  }

  stf_ctx->subtitle_frame->pts = sub->pts;
  stf_ctx->subtitle_frame->pkt_dts = in_ctx->init_pkt->dts;

  return 0;
}

void sub_to_frame_context_free(SubToFrameContext **stf_ctx)
{
  if (*stf_ctx == NULL) return;
  sws_freeContext((*stf_ctx)->sws_ctx);
  av_frame_unref((*stf_ctx)->subtitle_frame);
  av_frame_free(&(*stf_ctx)->subtitle_frame);
  free(*stf_ctx);
  *stf_ctx = NULL;
}

BurnInFilterContext *burn_in_filter_context_init(ProcessingContext *proc_ctx,
  InputContext *in_ctx)
{
  int ret = 0;
  char v_args[512], s_args[512], *flt_str = "[in1][in2]overlay[out]";
  const char *pix_fmt;

  const AVFilter *buffersrc = avfilter_get_by_name("buffer");
  const AVFilter *buffersink = avfilter_get_by_name("buffersink");

  int v_ctx_idx = proc_ctx->ctx_map[proc_ctx->v_stream_idx];
  AVCodecContext *v_dec_ctx = in_ctx->dec_ctx[v_ctx_idx];
  AVStream *v_stream = in_ctx->fmt_ctx->streams[proc_ctx->v_stream_idx];

  BurnInFilterContext *burn_in_ctx = NULL;
  AVFilterInOut *outputs = NULL;
  AVFilterInOut *inputs = NULL;

  if (!(burn_in_ctx = malloc(sizeof(BurnInFilterContext)))) {
    fprintf(stderr, "Failed to allocate BurnInFilterContext.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  burn_in_ctx->v_buffersrc_ctx = NULL;
  burn_in_ctx->s_buffersrc_ctx = NULL;
  burn_in_ctx->buffersink_ctx = NULL;
  burn_in_ctx->filter_graph = NULL;
  burn_in_ctx->filtered_frame = NULL;
  burn_in_ctx->stf_ctx = NULL;

  if (!(outputs = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate outputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(outputs->next = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate outputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(inputs = avfilter_inout_alloc())) {
    fprintf(stderr, "Failed to allocate outputs.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(burn_in_ctx->filter_graph = avfilter_graph_alloc())) {
    fprintf(stderr, "Failed to allocate filter graph.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  snprintf(v_args, sizeof(v_args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    v_dec_ctx->width, v_dec_ctx->height, v_dec_ctx->pix_fmt,
    v_stream->time_base.num, v_stream->time_base.den,
    v_dec_ctx->sample_aspect_ratio.num,
    v_dec_ctx->sample_aspect_ratio.den);

  if ((ret = avfilter_graph_create_filter(&burn_in_ctx->v_buffersrc_ctx, buffersrc,
    "in1", v_args, NULL, burn_in_ctx->filter_graph)) < 0)
  {
    fprintf(stderr, "Failed to create buffer source.\n");
    goto end;
  }

  snprintf(s_args, sizeof(s_args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    v_dec_ctx->width, v_dec_ctx->height, v_dec_ctx->pix_fmt,
    v_stream->time_base.num, v_stream->time_base.den,
    v_dec_ctx->sample_aspect_ratio.num,
    v_dec_ctx->sample_aspect_ratio.den);

  if ((ret = avfilter_graph_create_filter(&burn_in_ctx->s_buffersrc_ctx, buffersrc,
    "in2", s_args, NULL, burn_in_ctx->filter_graph)) < 0)
  {
    fprintf(stderr, "Failed to create buffer source.\n");
    goto end;
  }

  if (!(burn_in_ctx->buffersink_ctx =
    avfilter_graph_alloc_filter(burn_in_ctx->filter_graph, buffersink, "out")))
  {
    fprintf(stderr, "Failed to create buffer sink.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  pix_fmt = av_get_pix_fmt_name(v_stream->codecpar->format);

  if ((ret = av_opt_set(burn_in_ctx->buffersink_ctx, "pixel_formats",
    pix_fmt, AV_OPT_SEARCH_CHILDREN)))
  {
    fprintf(stderr, "Failed to set pixel format on buffersink.\n");
    goto end;
  }

  if ((ret = avfilter_init_dict(burn_in_ctx->buffersink_ctx, NULL))) {
    fprintf(stderr, "Failed to initialize buffersink.\n");
    goto end;
  }

  outputs->name = av_strdup("in1");
  outputs->filter_ctx = burn_in_ctx->v_buffersrc_ctx;
  outputs->pad_idx = 0;

  outputs->next->name = av_strdup("in2");
  outputs->next->filter_ctx = burn_in_ctx->s_buffersrc_ctx;
  outputs->next->pad_idx = 0;
  outputs->next->next = NULL;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = burn_in_ctx->buffersink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  if ((ret = avfilter_graph_parse_ptr(burn_in_ctx->filter_graph,
    flt_str, &inputs, &outputs, NULL)) < 0)
  {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if ((ret = avfilter_graph_config(burn_in_ctx->filter_graph, NULL)) < 0) {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if (!(burn_in_ctx->filtered_frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  burn_in_ctx->filtered_frame->width = v_dec_ctx->width;
  burn_in_ctx->filtered_frame->height = v_dec_ctx->height;
  burn_in_ctx->filtered_frame->format = v_dec_ctx->pix_fmt;
  burn_in_ctx->filtered_frame->color_primaries = v_dec_ctx->color_primaries;
  burn_in_ctx->filtered_frame->color_trc = v_dec_ctx->color_trc;
  burn_in_ctx->filtered_frame->colorspace = v_dec_ctx->colorspace;
  burn_in_ctx->filtered_frame->chroma_location = v_dec_ctx->chroma_sample_location;
  burn_in_ctx->filtered_frame->color_range = v_dec_ctx->color_range;

  if (!(burn_in_ctx->stf_ctx = sub_to_frame_context_alloc(proc_ctx, in_ctx))) {
    fprintf(stderr,
      "Failed to initialize subtitle to frame converter context.\n");
    ret = -ENOMEM;
    goto end;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  if (ret < 0) {
    burn_in_filter_context_free(&burn_in_ctx);
    return NULL;
  }

  return burn_in_ctx;
}

void burn_in_filter_context_free(BurnInFilterContext **burn_in_ctx)
{
  if (!*burn_in_ctx) return;
  avfilter_graph_free(&(*burn_in_ctx)->filter_graph);
  av_frame_unref((*burn_in_ctx)->filtered_frame);
  av_frame_free(&(*burn_in_ctx)->filtered_frame);
  sub_to_frame_context_free(&(*burn_in_ctx)->stf_ctx);
  free(*burn_in_ctx);
  *burn_in_ctx = NULL;
}
