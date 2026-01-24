#pragma once

#include "types.h"

#include <sqlite3.h>

#define INACTIVE_STREAM -1

InputContext *open_input(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
void close_input(InputContext **in_ctx);
