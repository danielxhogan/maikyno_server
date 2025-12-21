#pragma once

#include "swr.h"
#include "fsc.h"

typedef struct ProcessingContext {
  SwrOutputContext **swr_out_ctx;
  FrameSizeConversionContext **fsc_ctx;
  int *idx_map;
  char **titles;
  int deinterlace;
  int burn_in_idx;
  int *renditions;
} ProcessingContext;
