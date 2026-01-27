#include "utils.h"

#include <sqlite3.h>

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#else
#error "Unsupported platform"
#endif

int get_core_count() {
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    #elif defined(__unix__) || defined(__APPLE__)
        return sysconf(_SC_NPROCESSORS_ONLN);
    #else
        return -1;
    #endif
}

const char *job_status_enum_to_string(enum ProcessJobStatus status)
{
  switch (status) {
    case PENDING: { return "pending"; }
    case PROCESSING: { return "processing"; }
    case COMPLETE: { return "complete"; }
    case FAILED: { return "failed"; }
    case ABORTED: { return "aborted"; }
    default: { return "SHOULD NOT REACH"; }
  }
}

int check_abort_status(const char *batch_id)
{
  int status, ret = 0;
  sqlite3 *db;

  char *check_abort_status_query =
    "SELECT aborted \
    FROM batches \
    WHERE id = ?;";
  sqlite3_stmt *check_abort_status_stmt = NULL;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, check_abort_status_query,
    -1, &check_abort_status_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare check abort status statement.\nError: %s\n",
      sqlite3_errmsg(db));
    if (ret > 0) ret = -ret;
    goto end;
  }

  sqlite3_bind_text(check_abort_status_stmt, 1, batch_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(check_abort_status_stmt)) != SQLITE_ROW) {
    fprintf(stderr,
      "Failed to check abort status for batch: %s\nError: %s\n",
      batch_id, sqlite3_errmsg(db));
    if (ret > 0) ret = -ret;
    goto end;
  }

  status = sqlite3_column_int(check_abort_status_stmt, 0);

  if (status) {
    printf("batch \"%s\" is aborted.\n", batch_id);
    ret = ABORTED;
  }

end:
  sqlite3_finalize(check_abort_status_stmt);
  sqlite3_close(db);
  return ret;
}

int update_pct_complete(int64_t pct, char *process_job_id)
{
  int ret;
  sqlite3 *db;

  char *update_pct_complete_query =
    "UPDATE process_jobs \
    SET pct_complete = ? \
    WHERE id = ?;";
  sqlite3_stmt *update_pct_complete_stmt = NULL;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, update_pct_complete_query,
    -1, &update_pct_complete_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare update pct_complete statement. \
      \nError: %s\n", sqlite3_errmsg(db));
    goto end;
  }

  sqlite3_bind_int(update_pct_complete_stmt, 1, pct);
  sqlite3_bind_text(update_pct_complete_stmt, 2, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(update_pct_complete_stmt)) != SQLITE_DONE) {
    fprintf(stderr,
      "Failed to update pct_complete for process_job: %s\nError: %s\n",
      process_job_id, sqlite3_errmsg(db));
    goto end;
  }

end:
  sqlite3_finalize(update_pct_complete_stmt);
  sqlite3_close(db);

  if (ret != SQLITE_DONE) { return -ret; }
  return 0;
}

int calculate_pct_complete(ProcessingContext *proc_ctx, char *process_job_id)
{
  int ret;
  int64_t duration, pct_complete;

  duration = av_rescale_q(proc_ctx->in_fmt_ctx->duration, AV_TIME_BASE_Q,
    proc_ctx->in_fmt_ctx->streams[proc_ctx->pkt->stream_index]->time_base);

  pct_complete = proc_ctx->pkt->pts * 100 / duration;
  printf("pct_complete: %ld%%\n", pct_complete);

  if ((ret = update_pct_complete(pct_complete, process_job_id)) < 0) {
    fprintf(stderr, "Failed to update pct_complete for process_job: %s\n",
      process_job_id);
    return ret;
  }

  return 0;
}

int update_process_job_status(char *process_job_id, enum ProcessJobStatus status)
{
  int ret = 0;
  sqlite3 *db;

  char *update_process_job_status_query =
    "UPDATE process_jobs \
    SET job_status = ? \
    WHERE id = ?;";
  sqlite3_stmt *update_process_job_status_stmt = NULL;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, update_process_job_status_query, -1,
    &update_process_job_status_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare update process job status statement. \
      \nError: %s\n", sqlite3_errmsg(db));
    goto end;
  }

  sqlite3_bind_text(update_process_job_status_stmt, 1,
    job_status_enum_to_string(status), -1, SQLITE_STATIC);

  sqlite3_bind_text(update_process_job_status_stmt, 2, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(update_process_job_status_stmt)) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update status for process_job: \
      %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
    goto end;
  }

end:
  sqlite3_finalize(update_process_job_status_stmt);
  sqlite3_close(db);

  if (ret != SQLITE_DONE) { return -ret; }
  return 0;
}
