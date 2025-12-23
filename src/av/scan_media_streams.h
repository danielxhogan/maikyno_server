#pragma once

#include "utils.h"

#include <sqlite3.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

int scan_media_streams(const char *movie_id);
