#pragma once

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <stdio.h>

typedef struct ProcessingContext ProcessingContext;
typedef struct StreamContext StreamContext;
