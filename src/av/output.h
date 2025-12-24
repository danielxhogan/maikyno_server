#pragma once

#include "input.h"
#include "hdr.h"
#include "fsc.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

typedef struct OutputContext {
  AVFormatContext *fmt_ctx;
  AVCodecContext **enc_ctx;
  SwrContext **swr_ctx;
  FrameSizeConversionContext **fsc_ctx;
  AVFrame **swr_frame;
  AVPacket *enc_pkt;
  uint64_t *nb_samples_encoded;
  int nb_selected_streams;
} OutputContext;

OutputContext *open_output(ProcessingContext *proc_ctx,
  InputContext *in_ctx, char *process_job_id, sqlite3 *db);
void close_output(OutputContext *out_ctx);
