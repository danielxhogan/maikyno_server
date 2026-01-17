#include "input.h"

int get_input_file(char **input_file, char *process_job_id, sqlite3 *db)
{
  int ret, len_input_file;
  const unsigned char *input_file_tmp, *end;

  char *select_video_path_query =
    "SELECT videos.real_path FROM process_jobs \
    JOIN videos ON process_jobs.video_id = videos.id \
    WHERE process_jobs.id = ?;";
  sqlite3_stmt *select_video_path_stmt;

  if ((ret = sqlite3_prepare_v2(db, select_video_path_query, -1,
    &select_video_path_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select video path statement.\nError%s\n",
      sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_video_path_stmt, 1, process_job_id, -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_video_path_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to get path for video for process_job: %s\nError: %s\n",
      process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if (!(input_file_tmp = sqlite3_column_text(select_video_path_stmt, 0))) {
    fprintf(stderr, "Failed to get column text for select video path query \
      for process_job: %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
    ret = -1;
    goto end;
  }

  for (end = input_file_tmp; *end; end++);
  len_input_file = end - input_file_tmp;

  if (!(*input_file = calloc(len_input_file + 1, sizeof(char))))
  {
    fprintf(stderr, "Failed to allocate input_file\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  memcpy(*input_file, input_file_tmp, len_input_file + 1);

end:
  sqlite3_finalize(select_video_path_stmt);
  return ret;
}

static int open_decoder(InputContext *in_ctx, int in_stream_idx, int ctx_idx)
{
  int ret = 0;
  const AVCodec *dec;

  AVStream *stream = in_ctx->fmt_ctx->streams[in_stream_idx];

  if (!(dec = avcodec_find_decoder(stream->codecpar->codec_id))) {
    fprintf(stderr, "Failed to find decoder.\n");
    ret = AVERROR(EINVAL);
    return ret;
  }

  if (!(in_ctx->dec_ctx[ctx_idx] = avcodec_alloc_context3(dec))) {
    fprintf(stderr, "Failed to allocate decoder context "
      "for stream_idx: %d.\n", stream->index);
    ret = AVERROR(ENOMEM);
    return ret;
  }

  if ((ret =
    avcodec_parameters_to_context(in_ctx->dec_ctx[ctx_idx],
      stream->codecpar)) < 0)
  {
    fprintf(stderr, "Failed to copy codec parameters to decoder context "
      "for stream_idx: %d.\n", stream->index);
    return ret;
  }

  if ((ret =
    avcodec_open2(in_ctx->dec_ctx[ctx_idx], dec, NULL)) < 0)
  {
    fprintf(stderr, "Failed to open decoder for stream_idx: %d.\n",
      stream->index);
    return ret;
  }

  return ret;
}

InputContext *open_input(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db)
{
  int ctx_idx, ret = 0;
  char *input_file = NULL;
  unsigned int in_stream_idx;
  AVDictionary *opts = NULL;

  InputContext *in_ctx = malloc(sizeof(InputContext));
  if (!in_ctx) {
    ret = ENOMEM;
    goto end;
  }

  in_ctx->fmt_ctx = NULL;
  in_ctx->dec_ctx = NULL;
  in_ctx->init_pkt = NULL;
  in_ctx->init_pkt_cpy = NULL;
  in_ctx->dec_frame = NULL;
  in_ctx->dec_sub = NULL;
  in_ctx->nb_selected_streams = proc_ctx->nb_selected_streams;

  if ((ret = get_input_file(&input_file, process_job_id, db)) < 0)
  {
    fprintf(stderr,
      "Failed to get input file for process_job: %s\n", process_job_id);
    goto end;
  }

  printf("Opening input file \"%s\".\n", input_file);

  if ((ret = av_dict_set(&opts, "probesize", "50000000", 0)) < 0) {
    fprintf(stderr, "Failed to set probesize option.\n");
    return NULL;
  }

  if ((ret = av_dict_set(&opts, "analyzeduration", "10000000", 0)) < 0) {
      fprintf(stderr, "Failed to set analyzeduration option.\n");
      return NULL;
  }

  if ((ret =
    avformat_open_input(&in_ctx->fmt_ctx, input_file, NULL, &opts)) < 0)
  {
    fprintf(stderr, "Failed to open input file \"%s\".\n", input_file);
    goto end;
  }

  if ((ret = avformat_find_stream_info(in_ctx->fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info for: %s\n",
      input_file);
    goto end;
  }

  if (!(in_ctx->dec_ctx =
    calloc(in_ctx->nb_selected_streams, sizeof(AVCodecContext *))))
  {
    fprintf(stderr, "Failed to allocate in_ctx->dec_ctx array.\n");
    goto end;
  }

  for (
    in_stream_idx = 0;
    in_stream_idx < in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    if (ctx_idx == INACTIVE_STREAM) { continue; }
    if (proc_ctx->passthrough_arr[ctx_idx]) { continue; }

    if ((ret = open_decoder(in_ctx, in_stream_idx, ctx_idx)) < 0) {
      fprintf(stderr, "Failed to open decoder for stream %d for process job: %s\n",
        in_stream_idx, process_job_id);
      goto end;
    }
  }

  if (!(in_ctx->init_pkt = av_packet_alloc())) {
    fprintf(stderr, "Failed to allocate AVPacket.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(in_ctx->dec_frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(in_ctx->dec_sub = av_mallocz(sizeof(AVSubtitle)))) {
    fprintf(stderr, "Failed to allocate AVSubtitle.\n");
    ret = AVERROR(ENOMEM);
    return NULL;
  }

end:
  free(input_file);

  if (ret < 0) {
    fprintf(stderr, "Libav Error: %s.\n", av_err2str(ret));
    close_input(&in_ctx);
    return NULL;
  }

  return in_ctx;
}

void close_input(InputContext **in_ctx)
{
  if (!*in_ctx) return;
  int i;

  if ((*in_ctx)->dec_ctx) {
    for (i = 0; i < (*in_ctx)->nb_selected_streams; i++) {
      avcodec_free_context(&(*in_ctx)->dec_ctx[i]);
    }
    free((*in_ctx)->dec_ctx);
  }

  avformat_close_input(&(*in_ctx)->fmt_ctx);

  if ((*in_ctx)->init_pkt) { av_packet_unref((*in_ctx)->init_pkt); }
  av_packet_free(&(*in_ctx)->init_pkt);
  if ((*in_ctx)->init_pkt_cpy) { av_packet_unref((*in_ctx)->init_pkt_cpy); }
  av_packet_free(&(*in_ctx)->init_pkt_cpy);
  av_frame_unref((*in_ctx)->dec_frame);
  av_frame_free(&(*in_ctx)->dec_frame);
  if ((*in_ctx)->dec_sub) { avsubtitle_free((*in_ctx)->dec_sub); }
  free(*in_ctx);
  *in_ctx = NULL;
}
