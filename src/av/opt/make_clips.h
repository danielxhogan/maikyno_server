#pragma once

#include "utils.h"

#include <libavformat/avformat.h>

#include <malloc.h>
#include <stdio.h>

#define LEN_CLIP_FILE_STEM 2
#define LEN_CLIP_FILE_STEM_STRING "2"

#define LEN_MKV_EXTENSION 4
#define MKV_EXTENSION ".mkv"

#define LEN_CLIP_FILENAME LEN_CLIP_FILE_STEM + LEN_MKV_EXTENSION


enum FIRST_DTS_SET {
  NOT_SET,
  SET
};

int make_clips(const char *in_file_path, const char *out_dir_path,
  const char *process_job_id);
