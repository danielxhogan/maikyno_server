#pragma once

#include "proc.h"
#include "input.h"
#include "output.h"
#include "utils.h"

// #include <libavfilter/buffersink.h>
// #include <libavfilter/buffersrc.h>

#include <sqlite3.h>

int process_media(const char *batch_id);
