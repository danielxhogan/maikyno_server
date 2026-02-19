#include "fmt.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"

int get_filter_string(char *flt_str, ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, enum AVPixelFormat in_pix_fmt)
{
  int libplacebo = 0, convert_pix_fmt = 0, convert_colorspace = 0, tonemap = 0,
    convert_chroma_loc = 0, convert_range = 0, prev_flt = 0;

  if (proc_ctx->fmt_hdr) {
    if (
      stream_ctx->dec_ctx->color_primaries == AVCOL_PRI_UNSPECIFIED ||
      stream_ctx->dec_ctx->color_trc == AVCOL_TRC_UNSPECIFIED ||
      stream_ctx->dec_ctx->colorspace == AVCOL_SPC_UNSPECIFIED
    ) {
      fprintf(stderr, "Failed to initialize format filter. Unspecified colorspace.\n");
      return -1; 
    }
  } else {
    libplacebo = 1;
  }

  if (in_pix_fmt != proc_ctx->fmt_pix_fmt) { convert_pix_fmt = 1; }

  if (!proc_ctx->fmt_hdr) {
    if (
      stream_ctx->dec_ctx->color_primaries != AVCOL_PRI_BT709 ||
      stream_ctx->dec_ctx->color_trc != AVCOL_TRC_BT709 ||
      stream_ctx->dec_ctx->colorspace != AVCOL_SPC_BT709
    ) {
      convert_colorspace = 1;
    }

    if (proc_ctx->hdr) { tonemap = 1; }

    if (stream_ctx->dec_ctx->chroma_sample_location != AVCHROMA_LOC_LEFT) {
      convert_chroma_loc = 1;
    }
  }

  if (proc_ctx->hdr) { convert_chroma_loc = 1; }

  if (stream_ctx->dec_ctx->color_range != AVCOL_RANGE_MPEG) {
    convert_range = 1;
  }

  if (!(convert_pix_fmt || convert_colorspace ||
    convert_chroma_loc || convert_range)
  ) { return NO_CONVERSION; }

  if (libplacebo) {
    strcat(flt_str, "libplacebo=");

    if (convert_pix_fmt) {
      prev_flt = 1;
      strcat(flt_str, "format=");
      strcat(flt_str, av_get_pix_fmt_name(proc_ctx->fmt_pix_fmt));
    }

    if (convert_colorspace) {
      if (prev_flt) { strcat(flt_str, ":"); }
      prev_flt = 1;
      if (tonemap) { strcat(flt_str, "tonemapping=hable:"); }
      strcat(flt_str, "color_primaries=bt709:color_trc=bt709:colorspace=bt709");
    }

    if (convert_chroma_loc) {
      if (prev_flt) { strcat(flt_str, ":"); }
      prev_flt = 1;
      strcat(flt_str, "chroma_location=left");
    }

    if (convert_range) {
      if (prev_flt) { strcat(flt_str, ":"); }
      prev_flt = 1;
      strcat(flt_str, "range=tv");
    }
  } else {
    if (convert_pix_fmt) {
      prev_flt = 1;
      strcat(flt_str, "format=");
      strcat(flt_str, av_get_pix_fmt_name(proc_ctx->fmt_pix_fmt));
    }

    if (convert_chroma_loc) {
      if (prev_flt) { strcat(flt_str, ","); }
      prev_flt = 1;

      if (!proc_ctx->hw_ctx && !strcmp(proc_ctx->rend0_enc_name, "libx265")) {
        strcat(flt_str, "zscale=p=2020:t=smpte2084:m=2020_ncl:c=topleft");
      } else {
        strcat(flt_str, "zscale=c=topleft");
      }
    }

    if (convert_range) {
      if (prev_flt) { strcat(flt_str, ":"); }
      strcat(flt_str, "r=tv");
    }
  }

  return 0;
}

FormatFilterContext *format_filter_context_init(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, enum AVPixelFormat in_pix_fmt, int *no_conversion)
{
  int ret = 0;
  char args[512], flt_str[1024];
  snprintf(flt_str, sizeof(flt_str), "");
  const char *out_pix_fmt;

  const AVFilter *buffersrc = avfilter_get_by_name("buffer");
  const AVFilter *buffersink = avfilter_get_by_name("buffersink");
  AVCodecContext *dec_ctx = stream_ctx->dec_ctx;
  AVStream *in_stream = stream_ctx->in_stream;

  FormatFilterContext *fmt_ctx = NULL;
  AVFilterInOut *outputs = NULL;
  AVFilterInOut *inputs = NULL;


  if ((ret = get_filter_string(flt_str, proc_ctx, stream_ctx, in_pix_fmt)) < 0) {
    fprintf(stderr, "Failed to get filter string for format filter.\n");
    return NULL;
  }

  printf("flt_str: %s\n", flt_str);

  if (ret == NO_CONVERSION) {
    *no_conversion = NO_CONVERSION;
    return NULL;
  }

  if (!(fmt_ctx = malloc(sizeof(FormatFilterContext)))) {
    fprintf(stderr, "Failed to allocate DeinterlaceFilterContext.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  fmt_ctx->buffersink_ctx = NULL;
  fmt_ctx->buffersrc_ctx = NULL;
  fmt_ctx->filter_graph = NULL;
  fmt_ctx->filtered_frame = NULL;

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

  if (!(fmt_ctx->filter_graph = avfilter_graph_alloc())) {
    fprintf(stderr, "Failed to allocate filter graph.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  snprintf(args, sizeof(args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    dec_ctx->width, dec_ctx->height, in_pix_fmt,
    in_stream->time_base.num, in_stream->time_base.den,
    dec_ctx->sample_aspect_ratio.num,
    dec_ctx->sample_aspect_ratio.den);

  if ((ret = avfilter_graph_create_filter(&fmt_ctx->buffersrc_ctx, buffersrc,
    "in", args, NULL, fmt_ctx->filter_graph)) < 0)
  {
    fprintf(stderr, "Failed to create buffer source.\n");
    goto end;
  }

  if (!(fmt_ctx->buffersink_ctx =
    avfilter_graph_alloc_filter(fmt_ctx->filter_graph, buffersink, "out")))
  {
    fprintf(stderr, "Failed to create buffer sink.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  out_pix_fmt = av_get_pix_fmt_name(proc_ctx->fmt_pix_fmt);

  if ((ret = av_opt_set(fmt_ctx->buffersink_ctx, "pixel_formats",
    out_pix_fmt, AV_OPT_SEARCH_CHILDREN)))
  {
    fprintf(stderr, "Failed to set pixel format on buffersink.\n");
    goto end;
  }

  if ((ret = avfilter_init_dict(fmt_ctx->buffersink_ctx, NULL))) {
    fprintf(stderr, "Failed to initialize buffersink.\n");
    goto end;
  }

  outputs->name = av_strdup("in");
  outputs->filter_ctx = fmt_ctx->buffersrc_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = fmt_ctx->buffersink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  if ((ret = avfilter_graph_parse_ptr(fmt_ctx->filter_graph,
    flt_str, &inputs, &outputs, NULL)) < 0)
  {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if ((ret = avfilter_graph_config(fmt_ctx->filter_graph, NULL)) < 0) {
    fprintf(stderr, "Failed to configure filter graph.\n");
    goto end;
  }

  if (!(fmt_ctx->filtered_frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  if (ret < 0) {
    format_filter_context_free(&proc_ctx->fmt_ctx);
    return NULL;
  }
  return fmt_ctx;
}

void format_filter_context_free(FormatFilterContext **fmt_ctx)
{
  if (!*fmt_ctx) return;
  avfilter_graph_free(&(*fmt_ctx)->filter_graph);
  av_frame_unref((*fmt_ctx)->filtered_frame);
  av_frame_free(&(*fmt_ctx)->filtered_frame);
  free(*fmt_ctx);
  *fmt_ctx = NULL;
}
