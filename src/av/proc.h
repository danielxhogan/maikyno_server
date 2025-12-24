#pragma once

#include "types.h"

#include <sqlite3.h>

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db);
int get_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
void processing_context_free(ProcessingContext **proc_ctx);
