#include "proc.h"
#include "rendition.h"
#include "burn_in.h"
#include "deint.h"
#include "volume.h"
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

StreamConfig *stream_config_alloc()
{
  StreamConfig *stream_cfg;
  if (!(stream_cfg = malloc(sizeof(StreamConfig)))) { return NULL; }

  stream_cfg->rend1_title = NULL;
  stream_cfg->rend2_title = NULL;
  stream_cfg->passthrough = 0;
  stream_cfg->renditions = 0;
  stream_cfg->rend1_gain_boost = 0;
  stream_cfg->rend2_gain_boost = 0;

  return stream_cfg;
}

void stream_config_free(StreamConfig **stream_cfg)
{
  if (!*stream_cfg) { return; }

  free((*stream_cfg)->rend1_title);
  free((*stream_cfg)->rend2_title);
  free(*stream_cfg);
  *stream_cfg = NULL;
}

StreamContext *stream_context_alloc()
{
  StreamContext *stream_ctx;
  if(!(stream_ctx = malloc(sizeof(StreamContext)))) { return NULL; }

  stream_ctx->codec = NULL;
  stream_ctx->codec_type = AVMEDIA_TYPE_UNKNOWN;

  stream_ctx->in_stream = NULL;
  stream_ctx->in_stream_idx = -1;
  stream_ctx->dec_ctx = NULL;

  stream_ctx->rend1_swr_out_ctx = NULL;
  stream_ctx->rend2_swr_out_ctx = NULL;
  stream_ctx->rend1_fsc_ctx = NULL;
  stream_ctx->rend2_fsc_ctx = NULL;
  stream_ctx->rend1_vol_ctx = NULL;
  stream_ctx->rend2_vol_ctx = NULL;

  stream_ctx->rend1_out_stream_idx = -1;
  stream_ctx->rend2_out_stream_idx = -1;
  stream_ctx->rend1_enc_ctx = NULL;
  stream_ctx->rend2_enc_ctx = NULL;

  return stream_ctx;
}

void stream_context_free(StreamContext **stream_ctx)
{
  if (!*stream_ctx) { return; }

  free((*stream_ctx)->codec);

  free(*stream_ctx);
  *stream_ctx = NULL;
}

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db)
{
  int ret = 0;
  unsigned int i;

  ProcessingContext *proc_ctx = malloc(sizeof(ProcessingContext));
  if(!proc_ctx) {
    return NULL;
  }

  proc_ctx->nb_out_streams = 0;

  proc_ctx->idx_map = NULL;

  proc_ctx->swr_out_ctx_arr = NULL;
  proc_ctx->fsc_ctx_arr = NULL;

  proc_ctx->last_sub_pts = 0;
  proc_ctx->tminus1_v_pts = 0;
  proc_ctx->tminus2_v_pts = 0;

  proc_ctx->vol_ctx_arr = NULL;

  // ************************************
  proc_ctx->nb_in_streams = 0;
  proc_ctx->nb_selected_streams = 0;
  proc_ctx->v_stream_idx = -1;
  proc_ctx->ctx_map = NULL;

  proc_ctx->stream_cfg_arr = NULL;
  proc_ctx->stream_ctx_arr = NULL;

  proc_ctx->tonemap = 0;
  proc_ctx->hdr = 0;
  proc_ctx->rend_ctx = NULL;

  proc_ctx->deint = 0;
  proc_ctx->deint_ctx = NULL;

  proc_ctx->burn_in_idx = -1;
  proc_ctx->first_sub = 0;
  proc_ctx->burn_in_ctx = NULL;

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

  if (!(proc_ctx->stream_cfg_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(StreamConfig *))))
  {
    fprintf(stderr, "Failed to allocate stream config array.\n");
    goto end;
  }

  for (i = 0; i < proc_ctx->nb_selected_streams; i++) {
    if (!(proc_ctx->stream_cfg_arr[i] = stream_config_alloc())) {
      fprintf(stderr, "Failed to allocate stream config index '%d'.\n", i);
      goto end;
    }
  }

  if (!(proc_ctx->stream_ctx_arr =
    calloc(proc_ctx->nb_selected_streams, sizeof(StreamContext *))))
  {
    fprintf(stderr, "Failed to allocate stream context array.\n");
    goto end;
  }

  for (i = 0; i < proc_ctx->nb_selected_streams; i++) {
    if (!(proc_ctx->stream_ctx_arr[i] = stream_context_alloc())) {
      fprintf(stderr, "Failed to allocate stream context index '%d'.\n", i);
      goto end;
    }
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

  if ((*proc_ctx)->swr_out_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_out_streams; i++) {
      swr_output_context_free(&(*proc_ctx)->swr_out_ctx_arr[i]);
    }
    free((*proc_ctx)->swr_out_ctx_arr);
  }

  if ((*proc_ctx)->fsc_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_out_streams; i++) {
      fsc_ctx_free(&(*proc_ctx)->fsc_ctx_arr[i]);
    }
    free((*proc_ctx)->fsc_ctx_arr);
  }

  deint_filter_context_free(&(*proc_ctx)->deint_ctx);
  burn_in_filter_context_free(&(*proc_ctx)->burn_in_ctx);

  if ((*proc_ctx)->vol_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_out_streams; i++) {
      volume_filter_context_free(&(*proc_ctx)->vol_ctx_arr[i]);
    }
    free((*proc_ctx)->vol_ctx_arr);
  }

  free((*proc_ctx)->rend_ctx);

  if ((*proc_ctx)->stream_cfg_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      stream_config_free(&(*proc_ctx)->stream_cfg_arr[i]);
    }
    free((*proc_ctx)->stream_cfg_arr);
  }

  free((*proc_ctx)->ctx_map);
  free((*proc_ctx)->idx_map);

  free(*proc_ctx);
  *proc_ctx = NULL;
}

int get_video_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, int *ctx_idx, int *out_stream_idx, sqlite3 *db)
{
  StreamConfig *stream_cfg;
  StreamContext *stream_ctx;

  char *select_video_info_query =
    "SELECT streams.stream_idx, \
      process_job_video_streams.title, \
      process_job_video_streams.passthrough, \
      process_job_video_streams.deinterlace, \
      process_job_video_streams.create_renditions, \
      process_job_video_streams.title2, \
      process_job_video_streams.tonemap \
    FROM process_job_video_streams \
    JOIN streams ON process_job_video_streams.stream_id = streams.id \
    WHERE process_job_video_streams.process_job_id = ?;";

  sqlite3_stmt *select_video_info_stmt = NULL;
  char *title, *title2, *end;
  int in_stream_idx, len_title, len_title2, ret = 0;

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
  stream_cfg = proc_ctx->stream_cfg_arr[*ctx_idx];
  stream_ctx = proc_ctx->stream_ctx_arr[*ctx_idx];


  in_stream_idx = sqlite3_column_int(select_video_info_stmt, 0);
  proc_ctx->v_stream_idx = in_stream_idx;
  proc_ctx->ctx_map[in_stream_idx] = *ctx_idx;
  proc_ctx->idx_map[in_stream_idx] = *out_stream_idx;
  stream_ctx->in_stream_idx = in_stream_idx;

  title = (char *) sqlite3_column_text(select_video_info_stmt, 1);

  if (title) {
    for (end = title; *end; end++);
    len_title = end - title;

    if (!(stream_cfg->rend1_title =
      calloc(len_title + 1, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate memory for title for stream: %d\n",
        in_stream_idx);
      ret = AVERROR(ENOMEM);
      goto end;
    }

    strncat(stream_cfg->rend1_title, title, len_title);
  }

  stream_cfg->passthrough = sqlite3_column_int(select_video_info_stmt, 2);
  proc_ctx->deint = sqlite3_column_int(select_video_info_stmt, 3);
  stream_cfg->renditions = sqlite3_column_int(select_video_info_stmt, 4);
  title2 = (char *) sqlite3_column_text(select_video_info_stmt, 5);

  if (title2) {
    for (end = title2; *end; end++);
    len_title2 = end - title2;

    if (!(stream_cfg->rend2_title =
      calloc(len_title2 + 1, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate memory for title2 for stream: %d\n",
        in_stream_idx);
      ret = AVERROR(ENOMEM);
      goto end;
    }

    strncat(stream_cfg->rend2_title, title2, len_title2);
  }

  proc_ctx->tonemap = sqlite3_column_int(select_video_info_stmt, 6);

  if (stream_cfg->renditions) { *out_stream_idx += 1; }
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
  StreamConfig *stream_cfg;
  StreamContext *stream_ctx;

  char *select_audio_stream_info_query =
    "SELECT streams.stream_idx, \
      process_job_audio_streams.title, \
      process_job_audio_streams.passthrough, \
      process_job_audio_streams.gain_boost, \
      process_job_audio_streams.create_renditions, \
      process_job_audio_streams.title2, \
      process_job_audio_streams.gain_boost2, \
      streams.codec \
    FROM process_job_audio_streams \
    JOIN streams ON process_job_audio_streams.stream_id = streams.id \
    WHERE process_job_audio_streams.process_job_id = ?;";

  sqlite3_stmt *select_audio_stream_info_stmt = NULL;
  char *title, *title2, *codec, *end;
  int in_stream_idx, len_title, len_title2, len_codec, ret = 0;

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

    stream_cfg = proc_ctx->stream_cfg_arr[*ctx_idx];
    stream_ctx = proc_ctx->stream_ctx_arr[*ctx_idx];
    stream_ctx->in_stream_idx = in_stream_idx;

    title = (char *) sqlite3_column_text(select_audio_stream_info_stmt, 1);

    if (title) {
      for (end = title; *end; end++);
      len_title = end - title;

      if (!(stream_cfg->rend1_title =
        calloc(len_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate memory for title for stream: %d\n",
          in_stream_idx);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strncat(stream_cfg->rend1_title, title, len_title);
    }

    stream_cfg->passthrough = sqlite3_column_int(select_audio_stream_info_stmt, 2);

    stream_cfg->rend1_gain_boost =
      sqlite3_column_int(select_audio_stream_info_stmt, 3);

    stream_cfg->renditions = sqlite3_column_int(select_audio_stream_info_stmt, 4);

    title2 = (char *) sqlite3_column_text(select_audio_stream_info_stmt, 5);

    if (title2) {
      for (end = title2; *end; end++);
      len_title2 = end - title2;

      if (!(stream_cfg->rend2_title =
        calloc(len_title2 + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate memory for title2 for stream '%d'\n",
          in_stream_idx);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strncat(stream_cfg->rend2_title, title2, len_title2);
    }

    stream_cfg->rend2_gain_boost =
      sqlite3_column_int(select_audio_stream_info_stmt, 6);

    codec = (char *) sqlite3_column_text(select_audio_stream_info_stmt, 7);

    for (end = codec; *end; end++);
    len_codec = end - codec;

    if (!(stream_ctx->codec =
      calloc(len_codec + 1, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate memory for codec for stream '%d'\n",
        in_stream_idx);
      ret = AVERROR(ENOMEM);
      goto end;
    }

    strncat(stream_ctx->codec, codec, len_codec);

    *out_stream_idx += 1;
    if (stream_cfg->renditions) { *out_stream_idx += 1; }
    *ctx_idx += 1;
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
  StreamConfig *stream_cfg;
  StreamContext *stream_ctx;

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

    stream_cfg = proc_ctx->stream_cfg_arr[*ctx_idx];
    stream_ctx = proc_ctx->stream_ctx_arr[*ctx_idx];
    stream_ctx->in_stream_idx = in_stream_idx;

    title = (char *) sqlite3_column_text(select_subtitle_stream_idx_stmt, 1);

    if (title) {
      for (end = title; *end; end++);
      len_title = end - title;

      if (!(stream_cfg->rend1_title =
        calloc(len_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate memory for title for stream: %d\n",
          in_stream_idx);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strncat(stream_cfg->rend1_title, title, len_title);
    }

    burn_in = sqlite3_column_int(select_subtitle_stream_idx_stmt, 2);
    if (
      burn_in &&
      proc_ctx->burn_in_idx < 0 &&
      !proc_ctx->stream_cfg_arr[0]->passthrough
    ) {
      proc_ctx->burn_in_idx = in_stream_idx;
    }
    else {
      stream_cfg->passthrough = 1;

      proc_ctx->idx_map[in_stream_idx] = *out_stream_idx;
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
    fprintf(stderr, "Failed to get video processing info.\n");
    fprintf(stderr, "process job: %s.\n", process_job_id);
    return ret;
  }

  if ((ret = get_audio_process_info(proc_ctx, process_job_id,
    &ctx_idx, &out_stream_idx, db)) < 0)
  {
    fprintf(stderr, "Failed to get audio processing info.\n");
    fprintf(stderr, "process job: %s.\n", process_job_id);
    return ret;
  }

  if ((ret = get_subtitle_process_info(proc_ctx, process_job_id,
    &ctx_idx, &out_stream_idx, db)) < 0)
  {
    fprintf(stderr, "Failed to get subtitle processing info.\n");
    fprintf(stderr, "process job: %s.\n", process_job_id);
    return ret;
  }

  proc_ctx->nb_out_streams = out_stream_idx + 1;

  return 0;
}

int processing_context_init(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, char *process_job_id)
{
  int in_stream_idx, ctx_idx, out_stream_idx, passthrough, i, j;
  StreamConfig *stream_cfg;
  StreamContext *stream_ctx;
  enum AVMediaType codec_type;
  AVCodecContext *dec_ctx, *enc_ctx;

  if (!(proc_ctx->swr_out_ctx_arr =
    calloc(proc_ctx->nb_out_streams, sizeof(SwrOutputContext *))))
  {
    fprintf(stderr, "Failed to allocate array for swr output contexts.\n");
    return -1;
  }

  if (!(proc_ctx->fsc_ctx_arr =
    calloc(proc_ctx->nb_out_streams, sizeof(FrameSizeConversionContext *))))
  {
    fprintf(stderr, "Failed to allocate array \
      for frame size conversion contexts.\n");
    return -1;
  }

  if (!(proc_ctx->vol_ctx_arr =
    calloc(proc_ctx->nb_out_streams, sizeof(VolumeFilterContext *))))
  {
    fprintf(stderr, "Failed to allocate volume filter context array.\n");
    return -1;
  }

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) proc_ctx->nb_in_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    if (ctx_idx == INACTIVE_STREAM) { continue; }

    stream_cfg = proc_ctx->stream_cfg_arr[ctx_idx];
    stream_ctx = proc_ctx->stream_ctx_arr[ctx_idx];

    passthrough = stream_cfg->passthrough;
    if (passthrough) { continue; }

    codec_type = in_ctx->fmt_ctx->streams[in_stream_idx]->codecpar->codec_type;
    if (codec_type != AVMEDIA_TYPE_AUDIO) { continue; }

    dec_ctx = in_ctx->dec_ctx[ctx_idx];
    out_stream_idx = proc_ctx->idx_map[in_stream_idx];

    if (stream_cfg->renditions) {
      if (
        strcmp(stream_ctx->codec, "ac3") ||
        stream_cfg->rend1_gain_boost > 0
      ) {
        j = 2;
      }
      else
      {
        j = 1;
        out_stream_idx += 1;
      }
    } else {
      j = 1;
    }

    for (i = 0; i < j; i++) {
      enc_ctx = out_ctx->enc_ctx_arr[out_stream_idx];

      if (!(proc_ctx->swr_out_ctx_arr[out_stream_idx] =
        swr_output_context_alloc(dec_ctx, enc_ctx)))
      {
        fprintf(stderr, "Failed to allocate swr output context for \
          input stream '%d'.\n", in_stream_idx);
        return -1;
      }

      if (!(proc_ctx->fsc_ctx_arr[out_stream_idx] = fsc_ctx_alloc(enc_ctx))) {
        fprintf(stderr, "Failed to allocate fsc context for \
          input stream '%d'.\n", in_stream_idx);
        return -1;
      }

      if (
        ((j == 2 && i == 0) || (j == 1 && !stream_cfg->renditions)) &&
        stream_cfg->rend1_gain_boost > 0
      ) {
        if (!(proc_ctx->vol_ctx_arr[out_stream_idx] =
          volume_filter_context_init(proc_ctx, out_ctx, ctx_idx,
            out_stream_idx, 0)))
        {
          fprintf(stderr, "Failed to allocate volume filter context.\n");
          fprintf(stderr, "input stream: '%d'.\n", in_stream_idx);
          return -1;
        }
      }
      if (
        ((j == 2 && i == 1) || (j == 1 && stream_cfg->renditions)) &&
        stream_cfg->rend2_gain_boost > 0
      ) {
        if (!(proc_ctx->vol_ctx_arr[out_stream_idx] =
          volume_filter_context_init(proc_ctx, out_ctx, ctx_idx,
            out_stream_idx, 1)))
        {
          fprintf(stderr, "Failed to allocate volume filter context.\n");
          fprintf(stderr, "input stream: '%d'.\n", in_stream_idx);
          return -1;
        }
      }

      out_stream_idx += 1;
    }
  }

  in_stream_idx = proc_ctx->v_stream_idx;
  ctx_idx = proc_ctx->ctx_map[in_stream_idx];

  stream_cfg = proc_ctx->stream_cfg_arr[ctx_idx];

  out_stream_idx = proc_ctx->idx_map[in_stream_idx];

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
    !proc_ctx->stream_cfg_arr[0]->passthrough
  ) {
    if (!(proc_ctx->burn_in_ctx =
      burn_in_filter_context_init(proc_ctx, in_ctx)))
    {
      fprintf(stderr, "Failed to allocate burn in filter context \
        for process job: %s\n", process_job_id);
      return -1;
    }
  }

  if (stream_cfg->renditions)
  {
    if (!(proc_ctx->rend_ctx =
      video_rendition_filter_context_init(proc_ctx, in_ctx->dec_ctx[ctx_idx],
        out_ctx->enc_ctx_arr[out_stream_idx], out_ctx->enc_ctx_arr[out_stream_idx + 1],
        in_ctx->fmt_ctx->streams[proc_ctx->v_stream_idx])))
    {
      fprintf(stderr, "Failed to allocate video rendition context for \
        input stream: %d\nprocess job: %s\n", proc_ctx->v_stream_idx, process_job_id);
      return -1;
    }
  }

  return 0;
}
