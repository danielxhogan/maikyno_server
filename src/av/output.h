#pragma once

#include "types.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
// #include <libavcodec/bsf.h>
#include <sqlite3.h>

int open_output(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
// int initialize_bsf(ProcessingContext *proc_ctx, AVCodecContext *enc_ctx, AVStream *out_stream);
