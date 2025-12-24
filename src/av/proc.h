#pragma once

#include "input.h"
#include "swr.h"
#include "fsc.h"
#include "utils.h"

#include <sqlite3.h>

typedef struct ProcessingContext {
  unsigned int nb_in_streams;
  unsigned int nb_selected_streams;
  unsigned int nb_out_streams;

  int *ctx_map;
  int *idx_map;

  char **stream_titles_arr;
  int *passthrough_arr;
  unsigned int deinterlace;
  int burn_in_idx;
  int *gain_boost_arr;
  int *renditions_arr;

  SwrOutputContext **swr_out_ctx_arr;
  FrameSizeConversionContext **fsc_ctx_arr;
} ProcessingContext;

ProcessingContext *processing_context_alloc(char *process_job_id, sqlite3 *db);
int get_processing_info(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db);
void processing_context_free(ProcessingContext **proc_ctx);
