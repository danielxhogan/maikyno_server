#pragma once

#include "input.h"
#include "output.h"
#include "scan_media_streams.h"
#include "utils.h"

#include <sqlite3.h>
#include <stdio.h>

int process_media(const char *batch_id);
