#pragma once

#include <sqlite3.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <uuid/uuid.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>

#define LEN_UUID_STRING (sizeof(uuid_t) * 2) + 5

int scan_media_streams(const char *movie_id);
