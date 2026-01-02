#include "output.h"
#include "input.h"

static int copy_chapters(AVFormatContext *out_fmt_ctx,
  AVFormatContext *in_fmt_ctx)
{
  AVChapter *in_chapter, *out_chapter;
  int ret = 0;

  if (!(out_fmt_ctx->chapters =
    av_calloc(in_fmt_ctx->nb_chapters, sizeof(AVChapter *))))
  {
      fprintf(stderr, "Failed to allocate output format chapters array.\n");
      ret = AVERROR(ENOMEM);
      return ret;
  }

  for (unsigned int i = 0; i < in_fmt_ctx->nb_chapters; i++)
  {
    in_chapter = in_fmt_ctx->chapters[i];
    if (!(out_chapter = av_mallocz(sizeof(AVChapter)))) {
      fprintf(stderr, "Failed to allocate out_chapter for chapter %d", i);
      ret = AVERROR(ENOMEM);
      return ret;
    }

    out_chapter->id = in_chapter->id;
    out_chapter->time_base = in_chapter->time_base;
    out_chapter->start = in_chapter->start;
    out_chapter->end = in_chapter->end;

    if ((ret = av_dict_copy(&out_chapter->metadata,
      in_chapter->metadata, 0)) < 0)
    {
      fprintf(stderr, "Failed to copy chapter metadata.\n");
      av_freep(&out_chapter);
      return ret;
    }
    out_fmt_ctx->chapters[i] = out_chapter;
    out_fmt_ctx->nb_chapters++;
  }
  return ret;
}

static int get_len_params_str(char *hdr_params_str, char *additional_params_str)
{
  char *end;
  int len_hdr_params_str = 0, len_additional_params_str = 0;

  if (additional_params_str) {
    for (end = additional_params_str; *end; end++);
    len_additional_params_str = end - additional_params_str;
  }

  if (hdr_params_str) {
    for (end = hdr_params_str; *end; end++);
    len_hdr_params_str = end - hdr_params_str;
  }

  return len_hdr_params_str + len_additional_params_str;
}

static int open_video_encoder(AVCodecContext **enc_ctx,
  AVStream *in_stream, char *in_filename)
{
  int len_params_str, ret = 0;
  const AVCodec *enc;
  HdrMetadataContext *hdr_ctx = NULL;
  char *params_str = NULL, *hdr_params_str = NULL, *additional_params_str;

  if (!(enc = avcodec_find_encoder_by_name("libx265"))) {
    fprintf(stderr, "Failed to find encoder.\n");
    ret = AVERROR_UNKNOWN;
    goto end;
  }

  if (!(*enc_ctx = avcodec_alloc_context3(enc))) {
    fprintf(stderr, "Failed to allocate encoder context.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  (*enc_ctx)->time_base = in_stream->time_base;
  (*enc_ctx)->framerate = in_stream->avg_frame_rate;

  (*enc_ctx)->width = in_stream->codecpar->width;
  (*enc_ctx)->height = in_stream->codecpar->height;
  (*enc_ctx)->pix_fmt = in_stream->codecpar->format;

  (*enc_ctx)->color_range = in_stream->codecpar->color_range;
  (*enc_ctx)->color_primaries = in_stream->codecpar->color_primaries;
  (*enc_ctx)->color_trc = in_stream->codecpar->color_trc;
  (*enc_ctx)->colorspace = in_stream->codecpar->color_space;
  (*enc_ctx)->chroma_sample_location = in_stream->codecpar->chroma_location;

  // (*enc_ctx)->color_range = AVCOL_RANGE_UNSPECIFIED;
  // (*enc_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;
  // (*enc_ctx)->color_primaries = AVCOL_PRI_UNSPECIFIED;
  // (*enc_ctx)->color_trc = AVCOL_TRC_UNSPECIFIED;
  // (*enc_ctx)->colorspace = AVCOL_SPC_UNSPECIFIED;
  // (*enc_ctx)->chroma_sample_location = AVCHROMA_LOC_LEFT;

  printf("in_stream->codecpar->format: %d\n", in_stream->codecpar->format);
  printf("in_stream->codecpar->color_range: %d\n", in_stream->codecpar->color_range);
  printf("in_stream->codecpar->color_primaries: %d\n", in_stream->codecpar->color_primaries);
  printf("in_stream->codecpar->color_trc: %d\n", in_stream->codecpar->color_trc);
  printf("in_stream->codecpar->color_space: %d\n", in_stream->codecpar->color_space);
  printf("in_stream->codecpar->chroma_location: %d\n", in_stream->codecpar->chroma_location);

  (*enc_ctx)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (!(hdr_ctx = hdr_ctx_alloc())) {
    fprintf(stderr, "Failed to allocate hdr metadata.\n");
    ret = -ENOMEM;
    goto end;
  }

  if ((ret = extract_hdr_metadata(hdr_ctx, in_filename)) < 0) {
    fprintf(stderr, "Failed to extract hdr metadata.\n");
    goto end;
  }

  if ((ret = inject_hdr_metadta(hdr_ctx, *enc_ctx, &hdr_params_str)) < 0) {
    fprintf(stderr, "Failed to inject hdr metadata.\n");
    goto end;
  }

  additional_params_str =
    "pools=3:keyint=120:min-keyint=120:no-open-gop=true:no-scenecut=true";
  len_params_str = get_len_params_str(hdr_params_str, additional_params_str);

  if (!(params_str = calloc(len_params_str + 1, sizeof(char))))
  {
    fprintf(stderr, "Failed to allocate params_str\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (hdr_params_str) { strcat(params_str, hdr_params_str); }
  strcat(params_str, additional_params_str);

  if ((ret = av_opt_set((*enc_ctx)->priv_data,
    "x265-params", params_str, 0)) < 0)
  {
    fprintf(stderr, "Failed to set x265-params.\n");
    goto end;
  }

  if ((ret = av_opt_set((*enc_ctx)->priv_data, "crf", "20", 0)) < 0) {
    fprintf(stderr, "Failed to set crf on video encoder for video: %s\n",
      in_filename);
    goto end;
  }

  if ((ret = av_opt_set((*enc_ctx)->priv_data, "preset", "ultrafast", 0)) < 0) {
    fprintf(stderr, "Failed to set preset on video encoder for video: %s\n",
      in_filename);
    goto end;
  }

  if ((ret = av_opt_set((*enc_ctx)->priv_data, "tune", "grain", 0)) < 0) {
    fprintf(stderr, "Failed to set tune on video encoder for video: %s\n",
      in_filename);
      goto end;
  }

  if ((ret = avcodec_open2(*enc_ctx, enc, NULL)) < 0) {
    fprintf(stderr, "Failed to open encoder.\n");
    goto end;
  }

end:
  hdr_ctx_free(hdr_ctx);
  free(hdr_params_str);
  free(params_str);

  if (ret < 0) { return ret; }
  return 0;
}

// static int select_channel_layout(AVCodecContext *enc_ctx,
//   AVChannelLayout *preferred_layout)
// {
//   const AVChannelLayout *layouts = NULL, *current_layout;
//   char preferred_layout_name[64], current_layout_name[64];
//   int preferred_nb_channels, ret = 0;

//   if ((ret = avcodec_get_supported_config(enc_ctx, NULL,
//     AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, (const void **) &layouts, NULL)) < 0)
//   {
//     fprintf(stderr, "Failed to get supported channel layouts.\n");
//     return ret;
//   }

//   if ((ret = av_channel_layout_describe(preferred_layout,
//     preferred_layout_name, sizeof(preferred_layout_name))) < 0)
//   {
//     fprintf(stderr, "Failed to get name for preferred layout.\n");
//     return ret;
//   }

//   printf("preferred_layout: %s\n", preferred_layout_name);

//   if (!layouts) {
//     printf("No supported channel layouts list found. "
//       "Attempting to set preferred layout.\n");

//     if ((ret = av_channel_layout_copy(&enc_ctx->ch_layout,
//       preferred_layout)) < 0)
//     {
//       fprintf(stderr, "Failed to copy preferred channel layout."
//         "Attempting to set stereo channel layout\n");

//       goto set_stereo;
//     }
    
//     printf("Channel layout set to %s.\n", preferred_layout_name);
//     return 0;
//   }

//   printf("Checking if preferred layout is supported by encoder.\n");

//   current_layout = layouts;

//   while (current_layout->nb_channels) {
//     if ((ret = av_channel_layout_describe(current_layout,
//       current_layout_name, sizeof(current_layout_name))) < 0)
//     {
//       fprintf(stderr, "Failed to get name for current layout.\n");
//       return ret;
//     }

//     printf("current_layout_name: %s\n", current_layout_name);

//     if (!strcmp(current_layout_name, preferred_layout_name)) {
//       if ((ret =
//         av_channel_layout_copy(&enc_ctx->ch_layout, current_layout)) < 0)
//       {
//         fprintf(stderr, "Failed to copy preferred channel layout.\n");
//         return ret;
//       }

//       printf("Channel layout set to preferred layout.\n");
//       return 0;
//     }

//     current_layout++;
//   }

//   printf("Preferred layout not supported. Checking for supported layout "
//     "with equivalent number of channels.\n");

//   preferred_nb_channels = preferred_layout->nb_channels;
//   current_layout = layouts;

//   while (current_layout->nb_channels) {
//     if ((ret = av_channel_layout_describe(current_layout, current_layout_name,
//       sizeof(current_layout_name))) < 0)
//     {
//       fprintf(stderr, "Failed to get name of current layout.\n");
//       return ret;
//     }

//     printf("current_layout_name: %s\n", current_layout_name);

//     if (current_layout->nb_channels == preferred_nb_channels) {
//       if ((ret =
//         av_channel_layout_copy(&enc_ctx->ch_layout, current_layout)) < 0)
//       {
//         fprintf(stderr, "Failed to copy layout with preferred_nb_channels.\n");
//         return ret;
//       }

//       printf("Channel layout set to %s.\n", current_layout_name);
//       return 0;
//     }

//     current_layout++;
//   }

//   printf("No layout with equivalent number of channels supported. Attempting "
//   "to set channel layout to stereo.\n");

// set_stereo:
//   if ((ret = av_channel_layout_copy(&enc_ctx->ch_layout,
//     &(AVChannelLayout) AV_CHANNEL_LAYOUT_STEREO)) < 0)
//   {
//     fprintf(stderr, "Failed to copy stereo channel layout.\n");
//     return ret;
//   }

//   printf("Channel layout set to stereo.\n");
//   return 0;
// }

int select_sample_fmt(AVCodecContext *enc_ctx,
  enum AVSampleFormat preferred_fmt)
{
  const enum AVSampleFormat *formats = NULL;
  const char *preferred_fmt_name, *current_fmt_name;
  int i = 0, ret = 0;

  if ((ret = avcodec_get_supported_config(enc_ctx, NULL,
    AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **) &formats, NULL)) < 0)
  {
    fprintf(stderr, "Failed to get supported sample formats.\n");
    return ret;
  }

  if (!(preferred_fmt_name = av_get_sample_fmt_name(preferred_fmt))) {
    fprintf(stderr, "Failed to get name of preferred_fmt.\n");
    return AVERROR_UNKNOWN;
  }

  printf("preferred format: %s\n", preferred_fmt_name);

  if (!formats) {
    printf("No supported sample formats list found. "
      "Setting sample format to preferred sample format.\n");

    enc_ctx->sample_fmt = preferred_fmt;
    return 0;
  }

  printf("Checking if preferred sample format is supported by encoder.\n");

  while (formats[i] && formats[i] != AV_SAMPLE_FMT_NONE)
  {
    if (!(current_fmt_name = av_get_sample_fmt_name(formats[i]))) {
      fprintf(stderr, "Failed to get name of current_fmt_name.\n");
      return AVERROR_UNKNOWN;
    }

    printf("current_fmt_name: %s\n", current_fmt_name);

    if (!strcmp(current_fmt_name, preferred_fmt_name))
    {
      enc_ctx->sample_fmt = formats[i];
      printf("Sample format set to preferred sample format.\n");
      return 0;
    }

    i++;
  }

  printf("Preferred sample format not supported. "
    "Setting sample format to first supported sample format.\n");

  if (!(current_fmt_name = av_get_sample_fmt_name(formats[0]))) {
    fprintf(stderr, "Failed to get name of first supported sample format.\n");
    return AVERROR_UNKNOWN;
  }

  printf("first supported sample format: %s\n", current_fmt_name);
  enc_ctx->sample_fmt = formats[0];

  return 0;
}

static int open_audio_encoder(AVCodecContext **enc_ctx, AVStream *in_stream)
{
  int ret;
  const AVCodec *enc;
  AVChannelLayout *stereo = NULL;

  if (!(enc = avcodec_find_encoder_by_name("libfdk_aac"))) {
    fprintf(stderr, "Failed to find encoder.\n");
    ret = AVERROR_UNKNOWN;
    return ret;
  }

  if (!(*enc_ctx = avcodec_alloc_context3(enc))) {
    fprintf(stderr, "Failed to allocate encoder context.\n");
    ret = AVERROR(ENOMEM);
    return ret;
  }
  stereo = malloc(sizeof(AVChannelLayout));
  av_channel_layout_from_string(stereo, "stereo");

  if ((ret = av_channel_layout_copy(&(*enc_ctx)->ch_layout, stereo)) < 0) {
    fprintf(stderr, "Failed to set output stream channel layout \
      for input stream: %d.\nLibav Error: %s.\n",
      in_stream->index, av_err2str(ret));

    free(stereo);
    return ret;
  }
  free(stereo);

  // if ((ret = select_channel_layout(*enc_ctx,
  //   &in_stream->codecpar->ch_layout)) < 0)
  // {
  //   fprintf(stderr, "Failed to select channel layout.\n");
  //   return ret;
  // }

  if ((ret = select_sample_fmt(*enc_ctx,
    in_stream->codecpar->format)) < 0)
  {
    fprintf(stderr, "Failed to select sample format.\n");
    return ret;
  }

  (*enc_ctx)->sample_rate = in_stream->codecpar->sample_rate;
  (*enc_ctx)->bit_rate = in_stream->codecpar->bit_rate;

  if ((ret = avcodec_open2(*enc_ctx, enc, NULL)) < 0) {
    fprintf(stderr, "Failed to open encoder.\n");
    return ret;
  }

  return 0;
}

static int open_encoder(ProcessingContext *proc_ctx, OutputContext *out_ctx,
  int ctx_idx, int out_stream_idx, AVStream *in_stream, char *in_filename)
{
  int ret;
  enum AVMediaType stream_type = in_stream->codecpar->codec_type;

  if (stream_type == AVMEDIA_TYPE_VIDEO) {
    if ((ret =
      open_video_encoder(&out_ctx->enc_ctx[out_stream_idx], in_stream, in_filename)) < 0)
    {
      fprintf(stderr, "Failed to open video encoder for output stream.\n");
      return ret;
    }

    if (proc_ctx->renditions_arr[ctx_idx]) {
      out_stream_idx += 1;

      if ((ret = open_video_encoder(&out_ctx->enc_ctx[out_stream_idx],
        in_stream, in_filename)) < 0)
      {
        fprintf(stderr, "Failed to open video encoder for output stream.\n");
        return ret;
      }
    }
  }

  else if (stream_type == AVMEDIA_TYPE_AUDIO) {
    if (proc_ctx->renditions_arr[ctx_idx]) {
      out_stream_idx += 1;
    }

    if ((ret = open_audio_encoder(&out_ctx->enc_ctx[out_stream_idx],
      in_stream)) < 0)
    {
      fprintf(stderr, "Failed to open audio encoder for output stream.\n");
      return ret;
    }
  }

  return 0;
}

static int init_stream(AVFormatContext *fmt_ctx,
  AVCodecContext *enc_ctx, AVStream *in_stream, char *title)
{
  int ret;
  AVStream *out_stream;

  if (!(out_stream = avformat_new_stream(fmt_ctx, NULL))) {
    fprintf(stderr, "Failed to allocate new output stream.\n");
    ret = AVERROR(ENOMEM);
    return ret;
  }

  if ((ret = av_dict_copy(&out_stream->metadata,
    in_stream->metadata, AV_DICT_DONT_OVERWRITE)) < 0)
  {
    fprintf(stderr, "Failed to copy video metadata.\n");
    return ret;
  }

  if (title) {
    if ((ret = av_dict_set(&out_stream->metadata, "title", title, 0)) < 0) {
      fprintf(stderr, "Failed to set title for output stream.\n");
      return ret;
    }
  }

  if (enc_ctx) {
    if ((ret =
      avcodec_parameters_from_context(out_stream->codecpar, enc_ctx)) < 0)
    {
      fprintf(stderr,
        "Failed to copy codec parameters from encoder context to stream.\n");
      return ret;
    }

    out_stream->r_frame_rate = in_stream->r_frame_rate;
    out_stream->avg_frame_rate = in_stream->avg_frame_rate;
  }
  else {
    if ((ret =
      avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) < 0)
    {
      fprintf(stderr,
        "Failed to copy codec paramets from input stream to output stream.\n");
      return ret;
    }
  }

  return 0;
}

int get_file_data(char **name, int *extra, char **media_dir_path, char **title,
  sqlite3 *db, char *process_job_id)
{
  *name = NULL;
  *media_dir_path = NULL;
  *title = NULL;

  int len_name, len_media_dir_path, len_title, ret = 0;
  char *tmp_name, *tmp_media_dir_path, *tmp_title, *select_video_info_query, *end;
  sqlite3_stmt *select_video_info_stmt = NULL;

  select_video_info_query =
    "SELECT videos.name, videos.extra, media_dirs.real_path, process_jobs.title \
    FROM process_jobs \
    JOIN videos ON process_jobs.video_id = videos.id \
    JOIN media_dirs ON videos.media_dir_id = media_dirs.id \
    WHERE process_jobs.id = ?;";

  if ((ret = sqlite3_prepare_v2(db, select_video_info_query, -1,
    &select_video_info_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare video info statement while opening \
       output for process job: %s.\nSqlite Error: %s.\n",
       process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_video_info_stmt, 1, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_video_info_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to step through video info stmt while opening \
      output for process job: %s.\nSqlite Error: %s.\n",
      process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  tmp_name = (char *) sqlite3_column_text(select_video_info_stmt, 0);
  *extra = sqlite3_column_int(select_video_info_stmt, 1);
  tmp_media_dir_path = (char *) sqlite3_column_text(select_video_info_stmt, 2);
  tmp_title = (char *) sqlite3_column_text(select_video_info_stmt, 3);

  for (end = tmp_name; *end; end++);
  len_name = end - tmp_name;

  if (!(*name = calloc(len_name, sizeof(char *))))
  {
    fprintf(stderr, "Failed to allocate name of file for process job: %s.\n",
      process_job_id);
    goto end;
  }

  memcpy(*name, tmp_name, len_name);

  for (end = tmp_media_dir_path; *end; end++);
  len_media_dir_path = end - tmp_media_dir_path;

  if (!(*media_dir_path = calloc(len_media_dir_path, sizeof(char *))))
  {
    fprintf(stderr, "Failed to allocate media dir path of file for process job: %s.\n",
      process_job_id);
    goto end;
  }

  memcpy(*media_dir_path, tmp_media_dir_path, len_media_dir_path);

  for (end = tmp_title; *end; end++);
  len_title = end - tmp_title;

  if (!(*title = calloc(len_title, sizeof(char *))))
  {
    fprintf(stderr, "Failed to allocate title of file for process job: %s.\n",
      process_job_id);
    goto end;
  }

  memcpy(*title, tmp_title, len_title);

end:
  sqlite3_finalize(select_video_info_stmt);

  if (ret < 0) {
    free(*name);
    *name = NULL;
    free(*media_dir_path);
    *media_dir_path = NULL;
    free(*title);
    *title = NULL;
    return ret;
  }

  return 0;
}

int make_output_filename_string(char **out_filename,
  char *name, char *media_dir_path, int extra, char *process_job_id)
{
  char *end;
  int len_name, len_media_dir_path;

  for (end = name; *end; end++);
  len_name = end - name;

  for (end = media_dir_path; *end; end++);
  len_media_dir_path = end - media_dir_path;

  if (extra) {
    if (!(*out_filename =
      calloc(len_media_dir_path + len_name + 14, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate output filename for video: %s \
        for process job: %s.\n", name, process_job_id);
      return -ENOMEM;
    }

    strncat(*out_filename, media_dir_path, len_media_dir_path);
    strcat(*out_filename, "/proc/extras/");
    strncat(*out_filename, name, len_name);
  }
  else {
    if (!(*out_filename =
      calloc(len_media_dir_path + len_name + 7, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate output filename for video: %s \
        for process job: %s.\n", name, process_job_id);
      return -ENOMEM;
    }

    strncat(*out_filename, media_dir_path, len_media_dir_path);
    strcat(*out_filename, "/proc/");
    strncat(*out_filename, name, len_name);
  }

  return 0;
}

int open_encoders_and_streams(ProcessingContext *proc_ctx,
  InputContext *in_ctx, OutputContext *out_ctx, char *process_job_id)
{
  int in_stream_idx, ctx_idx, out_stream_idx, ret = 0;
  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    if (proc_ctx->ctx_map[in_stream_idx] == INACTIVE_STREAM) { continue; }
    if (in_stream_idx == proc_ctx->burn_in_idx) { continue; }

    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    out_stream_idx = proc_ctx->idx_map[in_stream_idx];

    if (!proc_ctx->passthrough_arr[ctx_idx]) {
      if ((ret = open_encoder(proc_ctx, out_ctx, ctx_idx, out_stream_idx,
        in_ctx->fmt_ctx->streams[in_stream_idx], in_ctx->fmt_ctx->url)) < 0)
      {
        fprintf(stderr, "Failed to open encoder for output stream: %d.\n\
          process job: %s.\nLibav Error: %s.\n",
          in_stream_idx, process_job_id, av_err2str(ret));
        return ret;
      }
    }

    if ((ret = init_stream(out_ctx->fmt_ctx, out_ctx->enc_ctx[out_stream_idx],
      in_ctx->fmt_ctx->streams[in_stream_idx],
      proc_ctx->stream_titles_arr[ctx_idx])) < 0)
    {
      fprintf(stderr, "Failed to initialize stream for output stream: %d.\n\
        process_job: %s.\nLibav Error: %s.\n",
        in_stream_idx, process_job_id, av_err2str(ret));
      return ret;
    }

    if (proc_ctx->renditions_arr[ctx_idx]) {
      if ((ret = init_stream(out_ctx->fmt_ctx, out_ctx->enc_ctx[out_stream_idx + 1],
        in_ctx->fmt_ctx->streams[in_stream_idx],
        proc_ctx->stream_titles_arr[ctx_idx])) < 0)
      {
        fprintf(stderr, "Failed to initialize stream for output stream: %d.\n\
          process_job: %s.\nLibav Error: %s.\n",
          in_stream_idx, process_job_id, av_err2str(ret));
        return ret;
      }
    }
  }

  return 0;
}

OutputContext *open_output(ProcessingContext *proc_ctx, InputContext *in_ctx,
  char *process_job_id, sqlite3 *db)
{
  int extra, ret = 0;
  char *name = NULL, *media_dir_path = NULL, *title = NULL, *out_filename = NULL;
  OutputContext *out_ctx;

  if (!(out_ctx = malloc(sizeof(OutputContext)))) {
    ret = -ENOMEM;
    return NULL;
  }

  out_ctx->fmt_ctx = NULL;
  out_ctx->enc_ctx = NULL;
  out_ctx->enc_pkt = NULL;
  out_ctx->nb_out_streams = proc_ctx->nb_out_streams;

  if ((ret = get_file_data(&name, &extra, &media_dir_path, &title,
    db, process_job_id)) < 0)
  {
    fprintf(stderr, "Failed to get file data for process job: %s.\n",
      process_job_id);
    goto end;
  }

  if ((ret = make_output_filename_string(&out_filename,
    name, media_dir_path, extra, process_job_id)) < 0)
  {
    fprintf(stderr, "Failed to get output filename for process job: %s.\n",
      process_job_id);
    goto end;
  }

  printf("\nOpening output file \"%s\".\n", out_filename);

  if ((ret = avformat_alloc_output_context2(
    &out_ctx->fmt_ctx, NULL, NULL, out_filename)))
  {
    fprintf(stderr, "Failed to allocate output format context:\n video: %s\n\
      process job:%s\nLivav Error: %s\n", name, process_job_id, av_err2str(ret));
    goto end;
  }

  if ((ret = av_dict_copy(&out_ctx->fmt_ctx->metadata,
    in_ctx->fmt_ctx->metadata, AV_DICT_DONT_OVERWRITE)) < 0)
  {
    fprintf(stderr, "Failed to copy file metadata:\nvideo: %s\nprocess job: %s\n\
      Libav Error: %s\n", name, process_job_id, av_err2str(ret));
    goto end;
  }

  if (title) {
    if ((ret = av_dict_set(&out_ctx->fmt_ctx->metadata, "title", title, 0)) < 0) {
      fprintf(stderr, "Failed to set title for output format context.\n\
        Libav Error: %s\n", av_err2str(ret));
      goto end;
    }
  }

  if ((ret = copy_chapters(out_ctx->fmt_ctx, in_ctx->fmt_ctx)) < 0)
  {
    fprintf(stderr, "Failed to copy chapters:\n\
      video: %s\nprocess job: %s\n", name, process_job_id);
    goto end;
  }

  if (!(out_ctx->enc_ctx =
    calloc(out_ctx->nb_out_streams, sizeof(AVCodecContext *))))
  {
    fprintf(stderr, "Failed to allocate array for encoder contexts:\n\
      video: %s\nprocess job: %s\n", name, process_job_id);
    ret = -ENOMEM;
    goto end;
  }

  if ((ret = open_encoders_and_streams(proc_ctx,
    in_ctx, out_ctx, process_job_id)) < 0)
  {
    fprintf(stderr, "Failed to open encoders and streams for process job: %s.\n",
      process_job_id);
    goto end;
  }

  if (!(out_ctx->enc_pkt = av_packet_alloc())) {
    fprintf(stderr, "Failed to allocate output context packet.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(out_ctx->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    if ((ret =
      avio_open(&out_ctx->fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE)) < 0)
    {
      fprintf(stderr, "Failed to open output file.\n");
      goto end;
    }
  }

  if ((ret = avformat_write_header(out_ctx->fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to write header for output file.\n");
    goto end;
  }

end:
  free(out_filename);
  free(name);
  free(media_dir_path);
  free(title);

  if (ret < 0) {
    close_output(&out_ctx);
    return NULL;
  }

  return out_ctx;
}

void close_output(OutputContext **out_ctx)
{
  int i;

  if (!*out_ctx) { return; }

  if ((*out_ctx)->fmt_ctx && !((*out_ctx)->fmt_ctx->flags & AVFMT_NOFILE))
    avio_closep(&(*out_ctx)->fmt_ctx->pb);
  avformat_free_context((*out_ctx)->fmt_ctx);

  if ((*out_ctx)->enc_ctx) {
    for (i = 0; i < (*out_ctx)->nb_out_streams; i++) {
      avcodec_free_context(&(*out_ctx)->enc_ctx[i]);
    }
    free((*out_ctx)->enc_ctx);
  }

  av_packet_free(&(*out_ctx)->enc_pkt);
  free(*out_ctx);
  *out_ctx = NULL;
}
