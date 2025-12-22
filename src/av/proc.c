#include "proc.h"

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
    fprintf(stderr, "Failed to prepare select stream count statement.\nError%s\n",
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
      for process_job: %s\nError: %s.\n", process_job_id, sqlite3_errmsg(db));
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
    fprintf(stderr, "Failed to prepare select stream count statement.\nError%s\n",
      sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_stream_count_stmt, 1, process_job_id, -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_stream_count_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to get stream count for process_job: %s\nError: %s\n",
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
  int nb_in_streams, out_stream_idx = 0, ctx_idx = 0, i, ret = 0;

  ProcessingContext *proc_ctx = malloc(sizeof(ProcessingContext));
  if(!proc_ctx) {
    return NULL;
  }

  proc_ctx->nb_selected_streams = 0;
  proc_ctx->stream_titles_arr = NULL;
  proc_ctx->passthrough = NULL;
  proc_ctx->deinterlace = 0;
  proc_ctx->burn_in_idx = -1;
  proc_ctx->gain_boost = NULL;
  proc_ctx->renditions = NULL;
  proc_ctx->swr_out_ctx_arr = NULL;
  proc_ctx->fsc_ctx_arr = NULL;
  proc_ctx->ctx_map = NULL;
  proc_ctx->idx_map = NULL;
  proc_ctx->nb_out_streams = 0;

  if ((ret = nb_in_streams =
    get_input_file_nb_streams(process_job_id, db)) < 0)
  {
    fprintf(stderr, "Failed to get number of streams in input file.\n");
    goto end;
  }

  if (!(proc_ctx->idx_map = calloc(nb_in_streams, sizeof(int)))) {
    fprintf(stderr, "Failed to allocate map array.\n");
    goto end;
  }

  for (i = 0; i < nb_in_streams; i++) {
    proc_ctx->idx_map[i] = INACTIVE_STREAM;
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

  if (!(proc_ctx->passthrough =
    calloc(proc_ctx->nb_selected_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate passthrough array.\n");
    goto end;
  }

  if (!(proc_ctx->gain_boost =
    calloc(proc_ctx->nb_selected_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate gain boost array.\n");
    goto end;
  }

  if (!(proc_ctx->renditions =
    calloc(proc_ctx->nb_selected_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate renditions array.\n");
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

  if (!(proc_ctx->ctx_map = calloc(nb_in_streams, sizeof(int)))) {
    fprintf(stderr, "Failed to allocate context map.\n");
    goto end;
  }

  if (!(proc_ctx->idx_map = calloc(nb_in_streams, sizeof(int))))
  {
    fprintf(stderr, "Failed to allocate index map.\n");
    goto end;
  }

  return proc_ctx;

end:
  processing_context_free(proc_ctx);
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

  free((*proc_ctx)->passthrough);
  free((*proc_ctx)->gain_boost);
  free((*proc_ctx)->renditions);

  if ((*proc_ctx)->swr_out_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      swr_output_context_free((*proc_ctx)->swr_out_ctx_arr[i]);
    }
    free((*proc_ctx)->swr_out_ctx_arr);
  }

  if ((*proc_ctx)->fsc_ctx_arr) {
    for (i = 0; i < (*proc_ctx)->nb_selected_streams; i++) {
      fsc_ctx_free((*proc_ctx)->fsc_ctx_arr[i]);
    }
    free((*proc_ctx)->fsc_ctx_arr);
  }

  free((*proc_ctx)->ctx_map);
  free((*proc_ctx)->idx_map);

  free(*proc_ctx);
  *proc_ctx = NULL;
}
