#pragma once

#include "types.h"
#include "hdr.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <sqlite3.h>

OutputContext *open_output(ProcessingContext *proc_ctx,
  InputContext *in_ctx, char *process_job_id, sqlite3 *db);
void close_output(OutputContext **out_ctx);
