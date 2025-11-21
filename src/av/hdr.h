#pragma once

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/dovi_meta.h>

#include "output.h"

typedef struct HdrMetadataContext {
  AVMasteringDisplayMetadata *mdm;
  char *mdm_str;
  AVContentLightMetadata *cll;
  char *cll_str;
  AVDOVIMetadata *dovi;
} HdrMetadataContext;

HdrMetadataContext *hdr_ctx_alloc();

int extract_hdr_metadata(HdrMetadataContext *hdr_ctx, const char *filename);

int inject_hdr_metadta(HdrMetadataContext *hdr_ctx, AVCodecContext *enc_ctx,
  char **params_str);

void hdr_ctx_free(HdrMetadataContext *hdr_ctx);
