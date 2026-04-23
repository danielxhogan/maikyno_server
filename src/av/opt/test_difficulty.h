#pragma once

#include "hdr.h"
#include "utils.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>

#include <sqlite3.h>

#include <stdio.h>
#include <dirent.h>
#include <malloc.h>
#include <string.h>

#define LEN_CRF 2
#define STARTING_CRF 15
#define ENDING_CRF 15
#define CRF_STEP 5

enum VideoType {
  MOVIE_VERSION,
  MOVIE_EXTRA
};

const char *video_type_to_string(enum VideoType video_type) {
  switch (video_type) {
    case MOVIE_VERSION: return "movie_version";
    case MOVIE_EXTRA: return "movie_extra";
    default: return "SHOULD NOT REACH";
  }
}

int64_t test_difficulty(const char *clips_dir_path,
  const char *test_diff_dir_path, const char *process_job_id);
