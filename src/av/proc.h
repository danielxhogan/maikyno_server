#pragma once

#include "types.h"
#include "deint.h"
#include "burn_in.h"
#include "rendition.h"
#include "volume.h"

#include <sqlite3.h>

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db);
int get_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
int processing_context_init(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, char *process_job_id);
void processing_context_free(ProcessingContext **proc_ctx);
