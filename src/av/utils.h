#pragma once

// #include "input.h"

#include <sqlite3.h>
#include <uuid/uuid.h>

enum ProcessJobStatus {
  PENDING,
  PROCESSING,
  COMPLETE,
  FAILED,
  ABORTED = -1024
};

#define LEN_UUID_STRING (sizeof(uuid_t) * 2) + 5

int get_core_count();
const char *job_status_enum_to_string(enum ProcessJobStatus status);
int check_abort_status(const char *batch_id);
int update_pct_complete(int64_t pct, char *process_job_id);
int update_process_job_status(char *process_job_id, enum ProcessJobStatus status);
