#pragma once

#include "input.h"

#include <sqlite3.h>
#include <stdint.h>
#include <string.h>

enum ProcessJobStatus {
  PENDING,
  PROCESSING,
  COMPLETE,
  FAILED,
  ABORTED = -1024
};

int get_core_count();
const char *job_status_enum_to_string(enum ProcessJobStatus status);
int check_abort_status(const char *batch_id);
int calculate_pct_complete(InputContext *in_ctx, char *process_job_id);
int update_pct_complete(int64_t pct, char *process_job_id);
