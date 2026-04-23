#pragma once

#include "common.h"

#include "swr.h"
#include "fsc.h"

#include "fmt.h"
#include "deint.h"
#include "burn_in.h"
#include "rendition.h"
#include "volume.h"
#include "input.h"

#include <sqlite3.h>

typedef struct StreamContext {
  char *codec;
  enum AVMediaType codec_type;
  int passthrough;
  int renditions;

  int in_stream_idx;
  AVStream *in_stream;
  AVCodecContext *dec_ctx;

  int transcode_rend0;
  AVCodecContext *rend0_enc_ctx;
  AVCodecContext *rend1_enc_ctx;

  char *rend0_title;
  char *rend1_title;
  int rend0_out_stream_idx;
  int rend1_out_stream_idx;
  AVStream *rend0_out_stream;
  AVStream *rend1_out_stream;

  SwrOutputContext *rend0_swr_out_ctx;
  SwrOutputContext *rend1_swr_out_ctx;

  FrameSizeConversionContext *rend0_fsc_ctx;
  FrameSizeConversionContext *rend1_fsc_ctx;

  int rend0_gain_boost;
  int rend1_gain_boost;
  VolumeFilterContext *rend0_vol_ctx;
  VolumeFilterContext *rend1_vol_ctx;
} StreamContext;

typedef struct ProcessingContext {
  unsigned int nb_in_streams;
  unsigned int nb_selected_streams;
  unsigned int v_stream_idx;
  int *ctx_map;

  AVFormatContext *in_fmt_ctx;
  AVFormatContext *out_fmt_ctx;

  StreamContext **stream_ctx_arr;

  enum AVCodecID rend0_codec;
  enum AVCodecID rend1_codec;
  int rend0_hwaccel;
  int rend1_hwaccel;

  enum AVHWDeviceType hw_type;
  AVBufferRef *hw_ctx;
  enum AVPixelFormat hw_pix_fmt;

  int hw_dec;
  char *rend0_enc_name;
  int rend0_hw_enc;
  // AVBSFContext *bsf_ctx;
  char *rend1_enc_name;
  int rend1_hw_enc;

  enum AVPixelFormat fmt_pix_fmt;
  int fmt_hdr;
  enum AVPixelFormat rend0_pix_fmt;
  int rend0_hdr;
  enum AVPixelFormat rend1_pix_fmt;
  int rend1_hdr;
  FormatFilterContext *fmt_ctx;

  int deint;
  DeinterlaceFilterContext *deint_ctx;

  int burn_in_idx;
  int first_sub;
  int64_t last_sub_pts;
  BurnInFilterContext *burn_in_ctx;

  int tonemap;
  int hdr;
  int dovi;
  RenditionFilterContext *rend_ctx;

  AVPacket *pkt;
  AVPacket *pkt_cpy;
  // AVPacket *bsf_pkt;
  AVFrame *frame;
  AVFrame *frame_cpy;
  AVFrame *sw_frame;
  AVFrame *rend0_hw_frame;
  AVFrame *rend1_hw_frame;
  AVSubtitle *sub;
} ProcessingContext;

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db);
int get_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
int hw_context_init(ProcessingContext *proc_ctx);
int processing_context_init(ProcessingContext *proc_ctx);
void processing_context_free(ProcessingContext **proc_ctx);
