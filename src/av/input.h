#pragma once

#include "proc.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

#include <sqlite3.h>

#define INACTIVE_STREAM -1

typedef struct InputContext {
  AVFormatContext *fmt_ctx;
  AVCodecContext **dec_ctx;
  AVPacket *init_pkt;
  AVFrame *dec_frame;
} InputContext;

InputContext *open_input(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
void close_input(InputContext *in_ctx);
