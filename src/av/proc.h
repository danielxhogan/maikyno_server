#pragma once

#include "swr.h"
#include "fsc.h"
#include "utils.h"

#include <sqlite3.h>

typedef struct ProcessingContext {
  int nb_selected_streams;
  char **stream_titles_arr;
  int *passthrough;
  int deinterlace;
  int burn_in_idx;
  int *gain_boost;
  int *renditions;

  SwrOutputContext **swr_out_ctx_arr;
  FrameSizeConversionContext **fsc_ctx_arr;

  int *ctx_map;
  int *idx_map;
  int nb_out_streams;
} ProcessingContext;

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db);
void processing_context_free(ProcessingContext **proc_ctx);
