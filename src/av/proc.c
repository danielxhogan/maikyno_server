#include "proc.h"

int get_input_file_nb_streams(char *process_job_id, sqlite3 *db)
{
  int stream_count = 0, ret;
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
    stream_count += 1;
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
  return stream_count;
}
