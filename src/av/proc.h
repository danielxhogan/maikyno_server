#pragma once

#include "swr.h"
#include "fsc.h"

#include <sqlite3.h>

typedef struct ProcessingContext {
  int *idx_map;
  int nb_selected_streams;
  char **titles;
  int *passthrough;
  int deinterlace;
  int burn_in_idx;
  int *gain_boost;
  int *renditions;
  SwrOutputContext **swr_out_ctx;
  FrameSizeConversionContext **fsc_ctx;
} ProcessingContext;

int get_input_file_nb_streams(char *process_job_id, sqlite3 *db);
