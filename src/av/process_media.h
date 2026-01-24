#pragma once

#include <libavfilter/buffersink.h>
#include <sqlite3.h>

int process_media(const char *batch_id);
