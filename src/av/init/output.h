#pragma once

#include "../utils/common.h"
#include <sqlite3.h>

int open_output(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db, char **out_filename);
// int initialize_bsf(ProcessingContext *proc_ctx, AVCodecContext *enc_ctx, AVStream *out_stream);
