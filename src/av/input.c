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

static int open_decoder(InputContext *in_ctx, int in_stream_idx)
{
  int ret = 0;
  const AVCodec *dec;

  AVStream *stream = in_ctx->fmt_ctx->streams[in_stream_idx];

  if (!(dec = avcodec_find_decoder(stream->codecpar->codec_id))) {
    fprintf(stderr, "Failed to find decoder.\n");
    ret = AVERROR(EINVAL);
    return ret;
  }

  if (!(in_ctx->dec_ctx[in_stream_idx] = avcodec_alloc_context3(dec))) {
    fprintf(stderr, "Failed to allocate decoder context "
      "for stream_idx: %d.\n", stream->index);
    ret = AVERROR(ENOMEM);
    return ret;
  }

  if ((ret =
    avcodec_parameters_to_context(in_ctx->dec_ctx[in_stream_idx],
      stream->codecpar)) < 0)
  {
    fprintf(stderr, "Failed to copy codec parameters to decoder context "
      "for stream_idx: %d.\n", stream->index);
    return ret;
  }

  if ((ret =
    avcodec_open2(in_ctx->dec_ctx[in_stream_idx], dec, NULL)) < 0)
  {
    fprintf(stderr, "Failed to open decoder for stream_idx: %d.\n",
      stream->index);
    return ret;
  }

  return ret;
}

int open_video_stream(InputContext *in_ctx, char *process_job_id,
  sqlite3 *db, int out_stream_idx)
{
  int ret = 0;

  char *select_video_info_query =
    "SELECT streams.stream_idx, \
      process_job_video_streams.title, \
      process_job_video_streams.passthrough \
    FROM process_job_video_streams \
    JOIN streams ON process_job_video_streams.stream_id = streams.id \
    WHERE process_job_video_streams.process_job_id = ?;";
  sqlite3_stmt *select_video_info_stmt = NULL;
  char *end;
  int in_stream_idx, passthrough;

  if ((ret = sqlite3_prepare_v2(db, select_video_info_query, -1,
    &select_video_info_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select process job video streams statement \
      for process job: %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
      ret = -ret;
      goto end;
  }

  sqlite3_bind_text(select_video_info_stmt, 1, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_video_info_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to get video stream id for process job: %s\n",
      process_job_id);
      ret = -ret;
    goto end;
  }

  in_stream_idx = sqlite3_column_int(select_video_info_stmt, 0);

  passthrough = sqlite3_column_int(select_video_info_stmt, 2);

  if (!passthrough) {
    printf("Output stream %d is being transcoded.\n", out_stream_idx);

    if ((ret = open_decoder(in_ctx, in_stream_idx)) < 0) {
      fprintf(stderr, "Failed to open decoder for stream %d\n",
        in_stream_idx);
      goto end;
    }
  } else {
    printf("Output stream %d is being passed through.\n", out_stream_idx);
  }

  out_stream_idx += 1;

end:
  sqlite3_finalize(select_video_info_stmt);

  if (ret < 0) { return ret; }
  return out_stream_idx;
}

int open_audio_streams(InputContext *in_ctx, char *process_job_id,
  sqlite3 *db, int out_stream_idx)
{
  int ret;

  char *select_audio_stream_info_query =
    "SELECT streams.stream_idx, \
    process_job_audio_streams.title, \
    process_job_audio_streams.passthrough \
    FROM process_job_audio_streams \
    JOIN streams ON process_job_audio_streams.stream_id = streams.id \
    WHERE process_job_audio_streams.process_job_id = ?;";
  sqlite3_stmt *select_audio_stream_info_stmt = NULL;
  char *end;
  int in_stream_idx, passthrough;

  if ((ret = sqlite3_prepare_v2(db, select_audio_stream_info_query, -1,
    &select_audio_stream_info_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select process job audio streams statement \
      for process job: %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
      ret = -ret;
      goto end;
  }

  sqlite3_bind_text(select_audio_stream_info_stmt, 1, process_job_id,
    -1, SQLITE_STATIC);

  while ((ret = sqlite3_step(select_audio_stream_info_stmt)) == SQLITE_ROW)
  {
    in_stream_idx = sqlite3_column_int(select_audio_stream_info_stmt, 0);

    passthrough = sqlite3_column_int(select_audio_stream_info_stmt, 2);

    if (!passthrough) {
      printf("Output stream %d is being transcoded.\n", out_stream_idx);
      if ((ret = open_decoder(in_ctx, in_stream_idx)) < 0) {
        fprintf(stderr, "Failed to open decoder for stream %d for process job: %s\n",
          in_stream_idx, process_job_id);
        goto end;
      }
    } else {
      printf("Output stream %d is being passed through.\n", out_stream_idx);
    }

    out_stream_idx += 1;
  }

  if (ret != SQLITE_DONE) {
    ret = -ret;
  }

end:
  sqlite3_finalize(select_audio_stream_info_stmt);
  if (ret < 0) { return ret; }
  return out_stream_idx;
}

int open_subtitle_streams(InputContext *in_ctx, char *process_job_id,
  sqlite3 *db, int out_stream_idx)
{
  int ret;

  char *select_subtitle_stream_idx_query =
    "SELECT streams.stream_idx, \
    process_job_subtitle_streams.title, \
    process_job_subtitle_streams.burn_in \
    FROM process_job_subtitle_streams \
    JOIN streams ON process_job_subtitle_streams.stream_id = streams.id \
    WHERE process_job_subtitle_streams.process_job_id = ?;";
  sqlite3_stmt *select_subtitle_stream_idx_stmt = NULL;
  char *end;
  int in_stream_idx, burn_in;

  if ((ret = sqlite3_prepare_v2(db, select_subtitle_stream_idx_query, -1,
    &select_subtitle_stream_idx_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select process job subtitle streams statement \
      for process job: %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
      ret = -ret;
      goto end;
  }

  sqlite3_bind_text(select_subtitle_stream_idx_stmt, 1, process_job_id,
    -1, SQLITE_STATIC);

  while ((ret = sqlite3_step(select_subtitle_stream_idx_stmt)) == SQLITE_ROW)
  {
    in_stream_idx = sqlite3_column_int(select_subtitle_stream_idx_stmt, 0);

    burn_in = sqlite3_column_int(select_subtitle_stream_idx_stmt, 2);

    if (burn_in) {
      printf("Output stream %d is being burned in.\n", out_stream_idx);
      if ((ret = open_decoder(in_ctx, in_stream_idx)) < 0) {
        fprintf(stderr, "Failed to open decoder for stream %d for process job: %s\n",
          in_stream_idx, process_job_id);
        goto end;
      }
    } else {
      printf("Output stream %d is not being burned in.\n", out_stream_idx);
    }

    out_stream_idx += 1;
  }

end:
  sqlite3_finalize(select_subtitle_stream_idx_stmt);
  return ret;
}

InputContext *open_input(char *process_job_id, sqlite3 *db)
{
  int ret, out_stream_idx = 0;
  unsigned int i;
  char *input_file = NULL;
  int stream_count;

  InputContext *in_ctx = malloc(sizeof(InputContext));
  if (!in_ctx) {
    ret = ENOMEM;
    goto end;
  }

  in_ctx->fmt_ctx = NULL;
  in_ctx->dec_ctx = NULL;
  in_ctx->init_pkt = NULL;
  in_ctx->dec_frame = NULL;

  if ((ret = get_input_file(&input_file, process_job_id, db)) < 0)
  {
    fprintf(stderr,
      "Failed to get input file for process_job: %s\n", process_job_id);
    goto end;
  }

  printf("\nOpening input file \"%s\".\n", input_file);

  if ((ret =
    avformat_open_input(&in_ctx->fmt_ctx, input_file, NULL, NULL)) < 0)
  {
    fprintf(stderr, "Failed to open input video file: '%s'.\n", input_file);
    goto end;
  }

  if ((ret = avformat_find_stream_info(in_ctx->fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info for: %s\n",
      input_file);
    goto end;
  }

  if (!(in_ctx->dec_ctx =
    calloc(in_ctx->fmt_ctx->nb_streams, sizeof(AVCodecContext *))))
  {
    fprintf(stderr, "Failed to allocate in_ctx->dec_ctx array.\n");
    goto end;
  }

  if ((ret = out_stream_idx =
    open_video_stream(in_ctx, process_job_id, db, out_stream_idx)) < 0)
  {
    fprintf(stderr, "Failed to open video stream for process job: %s",
      process_job_id);
    goto end;
  }

  if ((ret = out_stream_idx =
    open_audio_streams(in_ctx, process_job_id, db, out_stream_idx)) < 0)
  {
    fprintf(stderr, "Failed to open video stream for process job: %s",
      process_job_id);
    goto end;
  }

  if ((ret = open_subtitle_streams(in_ctx,
    process_job_id, db, out_stream_idx)) < 0)
  {
    fprintf(stderr, "Failed to open subtitle stream for process job: %s",
      process_job_id);
    goto end;
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

end:
  free(input_file);

  if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) {
    fprintf(stderr, "\nLibav Error: %s\n", av_err2str(ret));
    return NULL;
  }

  return in_ctx;
}

void close_input(InputContext *in_ctx)
{
  if (!in_ctx) return;
  unsigned int i;

  if (in_ctx->dec_ctx) {
    for (i = 0; i < in_ctx->fmt_ctx->nb_streams; i++) {
      avcodec_free_context(&in_ctx->dec_ctx[i]);
    }
    free(in_ctx->dec_ctx);
  }

  avformat_close_input(&in_ctx->fmt_ctx);

  av_packet_free(&in_ctx->init_pkt);
  av_frame_free(&in_ctx->dec_frame);
  free(in_ctx);
}
