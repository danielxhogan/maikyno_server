#include "proc.h"
#include "input.h"

int get_input_file_nb_streams(char *process_job_id, sqlite3 *db)
{
  int nb_in_streams = 0, ret;

  char *select_input_stream_count_querey =
    "select streams.id \
    from streams \
    join videos on streams.video_id = videos.id \
    join process_jobs on process_jobs.video_id = videos.id \
    where process_jobs.id = ?;";
    sqlite3_stmt *select_input_stream_count_stmt = NULL;

  if ((ret = sqlite3_prepare_v2(db, select_input_stream_count_querey, -1,
    &select_input_stream_count_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select stream count statement.\nSqlite Error%s\n",
      sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_input_stream_count_stmt, 1,
    process_job_id, -1, SQLITE_STATIC);

  while ((ret = sqlite3_step(select_input_stream_count_stmt)) == SQLITE_ROW)
  {
    nb_in_streams += 1;
  }

  if (ret != SQLITE_DONE) {
    fprintf(stderr, "Failed to get number of streams in input file \
      for process_job: %s\nSqlite Error: %s.\n", process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

end:
  sqlite3_finalize(select_input_stream_count_stmt);
  if (ret < 0) { return ret; }
  return nb_in_streams;
}

int get_nb_selected_streams(char *process_job_id, sqlite3 *db)
{
  int nb_selected_streams = 0, ret;

  char *select_stream_count_query =
    "SELECT stream_count FROM process_jobs WHERE id = ?";
  sqlite3_stmt *select_stream_count_stmt = NULL;

  if ((ret = sqlite3_prepare_v2(db, select_stream_count_query, -1,
    &select_stream_count_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select stream count statement.\nSqlite Error%s\n",
      sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_stream_count_stmt, 1, process_job_id, -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_stream_count_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to get stream count for process_job: %s\nSqlite Error: %s\n",
      process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  nb_selected_streams = sqlite3_column_int(select_stream_count_stmt, 0);

end:
  sqlite3_finalize(select_stream_count_stmt);
  if (ret < 0) { return ret; }
  return nb_selected_streams;
}

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db)
{
  int ret = 0;
  unsigned int i;

  ProcessingContext *proc_ctx = malloc(sizeof(ProcessingContext));
  if(!proc_ctx) {
    return NULL;
  }

  proc_ctx->nb_in_streams = 0;
  proc_ctx->nb_selected_streams = 0;
  proc_ctx->nb_out_streams = 0;

  proc_ctx->v_stream_idx = -1;

  proc_ctx->ctx_map = NULL;
  proc_ctx->idx_map = NULL;

  proc_ctx->stream_titles_arr = NULL;
  proc_ctx->passthrough_arr = NULL;

  proc_ctx->swr_out_ctx_arr = NULL;
  proc_ctx->fsc_ctx_arr = NULL;

  proc_ctx->deint = 0;
  proc_ctx->deint_ctx = NULL;

  proc_ctx->burn_in_idx = -1;
  proc_ctx->burn_in_ctx = NULL;

  proc_ctx->gain_boost_arr = NULL;
  proc_ctx->renditions_arr = NULL;
  proc_ctx->rend_ctx_arr = NULL;

  if ((ret = proc_ctx->nb_in_streams =
    get_input_file_nb_streams(process_job_id, db)) < 0)
  {
    fprintf(stderr, "Failed to get number of streams in input file.\n");
    goto end;
  }

  if (!(proc_ctx->idx_map = calloc(proc_ctx->nb_in_streams, sizeof(int)))) {
    fprintf(stderr, "Failed to allocate map array.\n");
    goto end;
  }

  for (i = 0; i < proc_ctx->nb_in_streams; i++) {
    proc_ctx->idx_map[i] = INACTIVE_STREAM;
  }

  if (!(proc_ctx->ctx_map = calloc(proc_ctx->nb_in_streams, sizeof(int)))) {
    fprintf(stderr, "Failed to allocate context map.\n");
    goto end;
  }

  for (i = 0; i < proc_ctx->nb_in_streams; i++) {
    proc_ctx->ctx_map[i] = INACTIVE_STREAM;
  }

  if ((ret = proc_ctx->nb_selected_streams =
    get_nb_selected_streams(process_job_id, db)) < 0)
  {
    fprintf(stderr, "Failed to get stream count for process job: %s\n",
      process_job_id);
    goto end;
  }

  if (!(proc_ctx->stream_titles_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(char *))))
  {
    fprintf(stderr, "Failed to allocate titles array.\n");
    goto end;
  }

  if (!(proc_ctx->passthrough_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate passthrough array.\n");
    goto end;
  }

  if (!(proc_ctx->swr_out_ctx_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(SwrOutputContext *))))
  {
    fprintf(stderr, "Failed to allocate array for swr output contexts \
      for process job: %s\n", process_job_id);
    goto end;
  }

  if (!(proc_ctx->fsc_ctx_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(FrameSizeConversionContext *))))
  {
    fprintf(stderr, "Failed to allocate array for frame size conversion contexts \
      for process job: %s\n", process_job_id);
    goto end;
  }

  if (!(proc_ctx->gain_boost_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate gain boost array.\n");
    goto end;
  }

  if (!(proc_ctx->renditions_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate renditions array.\n");
    goto end;
  }

  if (!(proc_ctx->rend_ctx_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(RenditionFilterContext *))))
  {
    fprintf(stderr, "Failed to allocate renditions context array.\n");
    goto end;
  }

  return proc_ctx;

end:
  processing_context_free(&proc_ctx);
  return NULL;
}

void processing_context_free(ProcessingContext **proc_ctx)
{
  if (!*proc_ctx) { return; }
  unsigned int i;

  if ((*proc_ctx)->stream_titles_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      free((*proc_ctx)->stream_titles_arr[i]);
    }
    free((*proc_ctx)->stream_titles_arr);
  }

  free((*proc_ctx)->passthrough_arr);

  if ((*proc_ctx)->swr_out_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      swr_output_context_free(&(*proc_ctx)->swr_out_ctx_arr[i]);
    }
    free((*proc_ctx)->swr_out_ctx_arr);
  }

  if ((*proc_ctx)->fsc_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      fsc_ctx_free(&(*proc_ctx)->fsc_ctx_arr[i]);
    }
    free((*proc_ctx)->fsc_ctx_arr);
  }

  deint_filter_context_free(&(*proc_ctx)->deint_ctx);
  burn_in_filter_context_free(&(*proc_ctx)->burn_in_ctx);


  free((*proc_ctx)->gain_boost_arr);

  free((*proc_ctx)->renditions_arr);

  if ((*proc_ctx)->rend_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      rendition_filter_context_free(&(*proc_ctx)->rend_ctx_arr[i]);
    }
    free((*proc_ctx)->rend_ctx_arr);
  }

  free((*proc_ctx)->ctx_map);
  free((*proc_ctx)->idx_map);

  free(*proc_ctx);
  *proc_ctx = NULL;
}

int get_video_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, int *ctx_idx, int *out_stream_idx, sqlite3 *db)
{
  char *select_video_info_query =
    "SELECT streams.stream_idx, \
      process_job_video_streams.title, \
      process_job_video_streams.passthrough, \
      process_job_video_streams.deinterlace, \
      process_job_video_streams.create_renditions \
    FROM process_job_video_streams \
    JOIN streams ON process_job_video_streams.stream_id = streams.id \
    WHERE process_job_video_streams.process_job_id = ?;";

  sqlite3_stmt *select_video_info_stmt = NULL;
  char *title, *end;
  int in_stream_idx, len_title, ret = 0;

  if ((ret = sqlite3_prepare_v2(db, select_video_info_query, -1,
    &select_video_info_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select process job video streams statement\
      for process job: %s.\nSqlite Error: %s.\n", process_job_id, sqlite3_errmsg(db));
      ret = -ret;
      goto end;
  }

  sqlite3_bind_text(select_video_info_stmt, 1, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_video_info_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to get video stream id for process job: %s.\n\
      Sqlite Error: %s.\n", process_job_id, sqlite3_errmsg(db));
      ret = -ret;
    goto end;
  }

  *ctx_idx = 0;
  *out_stream_idx = 0;

  in_stream_idx = sqlite3_column_int(select_video_info_stmt, 0);
  proc_ctx->v_stream_idx = in_stream_idx;
  proc_ctx->ctx_map[in_stream_idx] = *ctx_idx;
  proc_ctx->idx_map[in_stream_idx] = *out_stream_idx;

  printf("\nInput stream %d is mapped to output stream %d.\n",
    in_stream_idx, *out_stream_idx);

  title = (char *) sqlite3_column_text(select_video_info_stmt, 1);

  if (title) {
    printf("Output stream %d has title \"%s\".\n", *out_stream_idx, title);

    for (end = title; *end; end++);
    len_title = end - title;

    if (!(proc_ctx->stream_titles_arr[*ctx_idx] =
      calloc(len_title + 1, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate memory for title for stream: %d\n",
        in_stream_idx);
      ret = AVERROR(ENOMEM);
      goto end;
    }

    strncat(proc_ctx->stream_titles_arr[*ctx_idx], title, len_title);
  }

  proc_ctx->passthrough_arr[*ctx_idx] =
    sqlite3_column_int(select_video_info_stmt, 2);

  proc_ctx->deint = sqlite3_column_int(select_video_info_stmt, 3);

  proc_ctx->renditions_arr[*ctx_idx] =
    sqlite3_column_int(select_video_info_stmt, 4);

  if (proc_ctx->renditions_arr[*ctx_idx]) { *out_stream_idx += 1; }
  *out_stream_idx += 1;
  *ctx_idx = 1;

end:
  sqlite3_finalize(select_video_info_stmt);
  if (ret < 0) { return ret; }
  return 0;
}

int get_audio_process_info(ProcessingContext *proc_ctx,
  char *process_job_id, int *ctx_idx, int *out_stream_idx, sqlite3 *db)
{
  printf("out_stream_idx: %d\n\n\n", *out_stream_idx);
  char *select_audio_stream_info_query =
    "SELECT streams.stream_idx, \
      process_job_audio_streams.title, \
      process_job_audio_streams.passthrough, \
      process_job_audio_streams.gain_boost, \
      process_job_audio_streams.create_renditions \
    FROM process_job_audio_streams \
    JOIN streams ON process_job_audio_streams.stream_id = streams.id \
    WHERE process_job_audio_streams.process_job_id = ?;";

  sqlite3_stmt *select_audio_stream_info_stmt = NULL;
  char *title, *end;
  int in_stream_idx, len_title, ret = 0;

  if ((ret = sqlite3_prepare_v2(db, select_audio_stream_info_query, -1,
    &select_audio_stream_info_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select process job audio streams statement \
      for process job: %s\nSqlite Error: %s\n", process_job_id, sqlite3_errmsg(db));
      ret = -ret;
      goto end;
  }

  sqlite3_bind_text(select_audio_stream_info_stmt, 1, process_job_id,
    -1, SQLITE_STATIC);

  while ((ret = sqlite3_step(select_audio_stream_info_stmt)) == SQLITE_ROW)
  {
    in_stream_idx = sqlite3_column_int(select_audio_stream_info_stmt, 0);
    proc_ctx->ctx_map[in_stream_idx] = *ctx_idx;
    proc_ctx->idx_map[in_stream_idx] = *out_stream_idx;
    printf("\nInput stream %d is mapped to output stream %d.\n",
      in_stream_idx, *out_stream_idx);

    title = (char *) sqlite3_column_text(select_audio_stream_info_stmt, 1);
    if (title) {
      printf("Output stream %d has title \"%s\".\n", *out_stream_idx, title);
      for (end = title; *end; end++);
      len_title = end - title;

      if (!(proc_ctx->stream_titles_arr[*ctx_idx] =
        calloc(len_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate memory for title for stream: %d\n",
          in_stream_idx);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strncat(proc_ctx->stream_titles_arr[*ctx_idx], title, len_title);
    }

    proc_ctx->passthrough_arr[*ctx_idx] =
      sqlite3_column_int(select_audio_stream_info_stmt, 2);

    proc_ctx->gain_boost_arr[*ctx_idx] =
      sqlite3_column_int(select_audio_stream_info_stmt, 3);

    proc_ctx->renditions_arr[*ctx_idx] =
      sqlite3_column_int(select_audio_stream_info_stmt, 4);

    *ctx_idx += 1;
    *out_stream_idx += 1;
    if (proc_ctx->renditions_arr[*ctx_idx]) { *out_stream_idx += 1; }
  }

  if (ret != SQLITE_DONE) {
    fprintf(stderr, "Failed to step through audio info query.\n\
      Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
  }

end:
  sqlite3_finalize(select_audio_stream_info_stmt);
  if (ret < 0) { return ret; }
  return 0;
}

int get_subtitle_process_info(ProcessingContext *proc_ctx,
  char *process_job_id, int *ctx_idx, int *out_stream_idx, sqlite3 *db)
{
  char *select_subtitle_stream_idx_query =
    "SELECT streams.stream_idx, \
    process_job_subtitle_streams.title, \
    process_job_subtitle_streams.burn_in \
    FROM process_job_subtitle_streams \
    JOIN streams ON process_job_subtitle_streams.stream_id = streams.id \
    WHERE process_job_subtitle_streams.process_job_id = ?;";

  sqlite3_stmt *select_subtitle_stream_idx_stmt = NULL;
  char *title, *end;
  int in_stream_idx, len_title, burn_in, ret = 0;

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
    proc_ctx->ctx_map[in_stream_idx] = *ctx_idx;

    title = (char *) sqlite3_column_text(select_subtitle_stream_idx_stmt, 1);
    if (title) {
      printf("\nOutput stream %d has title \"%s\".\n", *out_stream_idx, title);
      for (end = title; *end; end++);
      len_title = end - title;

      if (!(proc_ctx->stream_titles_arr[*ctx_idx] =
        calloc(len_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate memory for title for stream: %d\n",
          in_stream_idx);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strncat(proc_ctx->stream_titles_arr[*ctx_idx], title, len_title);
    }

    burn_in = sqlite3_column_int(select_subtitle_stream_idx_stmt, 2);
    if (burn_in) {
      proc_ctx->burn_in_idx = in_stream_idx;
    }
    else {
      proc_ctx->passthrough_arr[*ctx_idx] = 1;
      proc_ctx->idx_map[in_stream_idx] = *out_stream_idx;
      printf("\nInput stream %d is mapped to output stream %d.\n",
        in_stream_idx, *out_stream_idx);
      *out_stream_idx += 1;
    }

    *ctx_idx += 1;
  }

end:
  sqlite3_finalize(select_subtitle_stream_idx_stmt);
  if (ret < 0) { return ret; }
  return 0;
}

int get_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db)
{
  int ctx_idx = 0, out_stream_idx = 0, ret = 0;

  if ((ret = get_video_processing_info(proc_ctx, process_job_id,
    &ctx_idx, &out_stream_idx, db)) < 0)
  {
    fprintf(stderr, "Failed to get video processing info for\
      process job: %s.\n", process_job_id);
    return ret;
  }

  if ((ret = get_audio_process_info(proc_ctx, process_job_id,
    &ctx_idx, &out_stream_idx, db)) < 0)
  {
    fprintf(stderr, "Failed to get audio processing info for\
      process job: %s.\n", process_job_id);
    return ret;
  }

  if ((ret = get_subtitle_process_info(proc_ctx, process_job_id,
    &ctx_idx, &out_stream_idx, db)) < 0)
  {
    fprintf(stderr, "Failed to get subtitle processing info for\
      process job: %s.\n", process_job_id);
    return ret;
  }

  proc_ctx->nb_out_streams = out_stream_idx + 1;

  return 0;
}

int processing_context_init(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, char *process_job_id)
{
  int in_stream_idx, ctx_idx, out_stream_idx, passthrough;
  enum AVMediaType codec_type;
  AVCodecContext *dec_ctx, *enc_ctx;

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) proc_ctx->nb_in_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    if (ctx_idx == INACTIVE_STREAM) { continue; }

    passthrough = proc_ctx->passthrough_arr[ctx_idx];
    if (passthrough) { continue; }

    dec_ctx = in_ctx->dec_ctx[ctx_idx];

    out_stream_idx = proc_ctx->idx_map[in_stream_idx];
    enc_ctx = out_ctx->enc_ctx[out_stream_idx];

    codec_type = in_ctx->fmt_ctx->streams[in_stream_idx]->codecpar->codec_type;

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (proc_ctx->renditions_arr[ctx_idx]) { out_stream_idx += 1; }

      if (!(proc_ctx->swr_out_ctx_arr[ctx_idx] =
        swr_output_context_alloc(dec_ctx, enc_ctx)))
      {
        fprintf(stderr, "Failed to allocate swr output context for \
          input stream: %d\nprocess job: %s\n", in_stream_idx, process_job_id);
        return -1;
      }

      if (!(proc_ctx->fsc_ctx_arr[ctx_idx] = fsc_ctx_alloc(enc_ctx))) {
        fprintf(stderr, "Failed to allocate fsc context for \
          input stream: %d\nprocess job: %s\n", in_stream_idx, process_job_id);
        return -1;
      }
    }
   }

  ctx_idx = proc_ctx->ctx_map[proc_ctx->v_stream_idx];

  if (proc_ctx->deint) {
    if (!(proc_ctx->deint_ctx =
      deint_filter_context_init(in_ctx, proc_ctx->v_stream_idx, ctx_idx)))
    {
      fprintf(stderr, "Failed to allocate deinterlace filter context \
        for process job: %s\n", process_job_id);
      return -1;
    }
  }

  if (
    proc_ctx->burn_in_idx != -1 &&
    !proc_ctx->passthrough_arr[proc_ctx->v_stream_idx]
  ) {
    if (!(proc_ctx->burn_in_ctx = burn_in_filter_context_init(proc_ctx, in_ctx)))
    {
      fprintf(stderr, "Failed to allocate burn in filter context \
        for process job: %s\n", process_job_id);
      return -1;
    }
  }

  if (proc_ctx->renditions_arr[ctx_idx]) {
    if (!(proc_ctx->rend_ctx_arr[ctx_idx] =
      video_rendition_filter_context_init(in_ctx->dec_ctx[ctx_idx],
        in_ctx->fmt_ctx->streams[proc_ctx->v_stream_idx])))
    {
      fprintf(stderr, "Failed to allocate video rendition context for \
        input stream: %d\nprocess job: %s\n", proc_ctx->v_stream_idx, process_job_id);
      return -1;
    }
  }

  return 0;
}
