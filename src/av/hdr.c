#include "hdr.h"

static int max_len_mdm_str()
{
  char *max_mdm_str = "G(2147483647,2147483647)B(2147483647,2147483647)R(2147483647,2147483647)WP(2147483647,2147483647)L(2147483647,2147483647)";
  char *end;
  int len_max_mdm_str;

  for (end = max_mdm_str; *end; end++);
  len_max_mdm_str = end - max_mdm_str;

  return len_max_mdm_str;
}

#define MDM_STR_MAX_LEN max_len_mdm_str()

static int max_len_cll_str()
{
  char *max_cll_str = "2147483647,2147483647";
  char *end;
  int len_max_cll_str;

  for (end = max_cll_str; *end; end++);
  len_max_cll_str = end - max_cll_str;

  return len_max_cll_str;
}

#define CLL_STR_MAX_LEN max_len_cll_str()

static int len_hdr_params_str(int dovi)
{
  char *params_base, *end;
  int len_params_base, len_params;

  if (dovi) {
    params_base =
      "master-display=:max-cll=:vbv-maxrate=100000:vbv-bufsize=200000:";
  } else {
    params_base = "master-display=:max-cll=:";
  }

  for (end = params_base; *end; end++);
  len_params_base = end - params_base;

  len_params = len_params_base + MDM_STR_MAX_LEN + CLL_STR_MAX_LEN;

  return len_params;
}

#define DOVI_PARAMS_STR_LEN len_hdr_params_str(1)
#define HDR_PARAMS_STR_LEN len_hdr_params_str(0)

HdrMetadataContext *hdr_ctx_alloc()
{
  HdrMetadataContext *hdr_ctx = malloc(sizeof(HdrMetadataContext));
  if (!hdr_ctx) return NULL;
  hdr_ctx->mdm = NULL;
  hdr_ctx->mdm_str = NULL;
  hdr_ctx->cll = NULL;
  hdr_ctx->cll_str = NULL;
  hdr_ctx->dovi = NULL;
  return hdr_ctx;
}

int extract_hdr_metadata(HdrMetadataContext *hdr_ctx, const char *filename)
{
  AVFormatContext *fmt_ctx = NULL;
  AVStream *in_stream = NULL;
  AVCodecContext *dec_ctx = NULL;
  const AVCodec *dec;
  AVPacket *pkt = NULL;
  AVFrame *frame = NULL;
  int ret = 0, v_stream_idx = -1, frames_decoded = 0;
  AVFrameSideData *frame_sd;

  #define FRAME_DECODE_LIMIT 100

  if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
    fprintf(stderr, "Failed to open input video file: '%s'.\n", filename);
    goto end;
  }

  if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info.");
    goto end;
  }

  if ((ret = v_stream_idx =
    av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0)
  {
    fprintf(stderr, "Failed to find video stream in input file.\n");
    goto end;
  }

  in_stream = fmt_ctx->streams[v_stream_idx];

  if (!(dec = avcodec_find_decoder(in_stream->codecpar->codec_id))) {
    fprintf(stderr, "Failed to find decoder.\n");
    ret = AVERROR(EINVAL);
    goto end;
  }

  if (!(dec_ctx = avcodec_alloc_context3(dec))) {
    fprintf(stderr, "Failed to allocate decoder context.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if ((ret = avcodec_parameters_to_context(dec_ctx, in_stream->codecpar)) < 0) {
    fprintf(stderr, "Failed to copy codec parameters to decoder context.\n");
    goto end;
  }

  if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
    fprintf(stderr, "Failed to open decoder.\n");
    goto end;
  }

  if (!(pkt = av_packet_alloc())) {
    fprintf(stderr, "Failed to allocate AVPacket.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(frame = av_frame_alloc())) {
    fprintf(stderr, "Failed to allocate AVFrame.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  while (av_read_frame(fmt_ctx, pkt) >= 0 && frames_decoded < FRAME_DECODE_LIMIT)
  {
    if (pkt->stream_index != v_stream_idx) {
      av_packet_unref(pkt);
      continue;
    }

    if ((ret = avcodec_send_packet(dec_ctx, pkt)) < 0) {
      fprintf(stderr, "Failed to send video packet to decoder.\n");
      av_packet_unref(pkt);
      goto end;
    }

    while ((ret = avcodec_receive_frame(dec_ctx, frame)) >= 0)
    {
      frames_decoded++;

      if (!hdr_ctx->mdm) {
        frame_sd = av_frame_get_side_data(frame,
          AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);

        if (frame_sd) {
          hdr_ctx->mdm = av_mastering_display_metadata_alloc();
          memcpy(hdr_ctx->mdm, frame_sd->data,
            sizeof(AVMasteringDisplayMetadata));

          if (!(hdr_ctx->mdm_str =
            calloc(MDM_STR_MAX_LEN + 1, sizeof(char))))
          {
            fprintf(stderr, "Failed to allocate hdr_ctx->mdm_str\n");
            ret = AVERROR(ENOMEM);
            goto end;
          }

          snprintf(hdr_ctx->mdm_str,
            MDM_STR_MAX_LEN + 1,
            "G(%d,%d)B(%d,%d)R(%d,%d)WP(%d,%d)L(%d,%d)",
            hdr_ctx->mdm->display_primaries[1][0].num,
            hdr_ctx->mdm->display_primaries[1][1].num,
            hdr_ctx->mdm->display_primaries[2][0].num,
            hdr_ctx->mdm->display_primaries[2][1].num,
            hdr_ctx->mdm->display_primaries[0][0].num,
            hdr_ctx->mdm->display_primaries[0][1].num,
            hdr_ctx->mdm->white_point[0].num,
            hdr_ctx->mdm->white_point[1].num,
            hdr_ctx->mdm->max_luminance.num,
            hdr_ctx->mdm->min_luminance.num);
        }
      }

      if (!hdr_ctx->cll) {
        frame_sd = av_frame_get_side_data(frame,
          AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);

        if (frame_sd) {
          hdr_ctx->cll = av_content_light_metadata_alloc(NULL);
          memcpy(hdr_ctx->cll, frame_sd->data, sizeof(AVContentLightMetadata));

          if (!(hdr_ctx->cll_str =
            calloc(CLL_STR_MAX_LEN + 1, sizeof(char))))
          {
            fprintf(stderr, "Failed to allocate hdr_ctx->cll_str\n");
            ret = AVERROR(ENOMEM);
            goto end;
          }

          snprintf(hdr_ctx->cll_str, CLL_STR_MAX_LEN + 1, "%d,%d",
            hdr_ctx->cll->MaxCLL, hdr_ctx->cll->MaxFALL);
        }
      }

      if (!hdr_ctx->dovi) {
        frame_sd = av_frame_get_side_data(frame,
          AV_FRAME_DATA_DOVI_METADATA);

        if (frame_sd) {
          hdr_ctx->dovi = av_dovi_metadata_alloc(NULL);
          memcpy(hdr_ctx->dovi, frame_sd->data, sizeof(AVDOVIMetadata));
        }
      }

      av_frame_unref(frame);
      if (hdr_ctx->mdm && hdr_ctx->cll && hdr_ctx->dovi) break;
    }

    if ((ret < 0 && ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
      fprintf(stderr, "Failed to receive frame from decoder.: %d\n", ret);
      goto end;
    }

    if (hdr_ctx->mdm && hdr_ctx->cll && hdr_ctx->dovi) {
      break;
    }
  }

  if ((ret < 0 && ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to read frame from input file.\n");
    goto end;
  }

end:
  avformat_close_input(&fmt_ctx);
  avcodec_free_context(&dec_ctx);
  av_packet_free(&pkt);
  av_frame_free(&frame);

  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
  return ret;
}

int inject_hdr_metadta(HdrMetadataContext *hdr_ctx, AVCodecContext *enc_ctx,
  char **params_str)
{
  int ret;

  if (hdr_ctx->mdm && hdr_ctx->cll)
  {
    if (!av_packet_side_data_add(&enc_ctx->coded_side_data,
      &enc_ctx->nb_coded_side_data, AV_PKT_DATA_MASTERING_DISPLAY_METADATA,
      (uint8_t *) hdr_ctx->mdm, sizeof(AVMasteringDisplayMetadata), 0))
    {
      fprintf(stderr,
        "Failed to add mastering display metadata to encoder context.\n");
      return AVERROR_UNKNOWN;
    }

    if (!av_packet_side_data_add(&enc_ctx->coded_side_data,
      &enc_ctx->nb_coded_side_data, AV_PKT_DATA_CONTENT_LIGHT_LEVEL,
      (uint8_t *) hdr_ctx->cll, sizeof(AVContentLightMetadata), 0))
    {
      fprintf(stderr,
        "Failed to add content light level metadata to encoder context.\n");
      return AVERROR_UNKNOWN;
    }

    if (*params_str) {
      free(*params_str);
    }

    if (hdr_ctx->dovi) {
      if (!av_packet_side_data_add(&enc_ctx->coded_side_data,
        &enc_ctx->nb_coded_side_data, (enum AVPacketSideDataType) AV_FRAME_DATA_DOVI_METADATA,
        (uint8_t *) hdr_ctx->dovi, sizeof(AVDOVIMetadata), 0))
      {
        fprintf(stderr,
          "Failed to add content light level metadata to encoder context.\n");
        return AVERROR_UNKNOWN;
      }

      if ((ret = av_opt_set(enc_ctx->priv_data, "dolbyvision", "true", 0)) < 0) {
        fprintf(stderr, "Failed to set dolbyvision opt.\n");
        return ret;
      }

      if (!(*params_str = calloc(DOVI_PARAMS_STR_LEN + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate params_str\n");
        return AVERROR(ENOMEM);
      }

      snprintf(*params_str, DOVI_PARAMS_STR_LEN,
        "master-display=%s:max-cll=%s:vbv-maxrate=100000:vbv-bufsize=200000:",
        hdr_ctx->mdm_str, hdr_ctx->cll_str);
    }
    else {
      if (!(*params_str = calloc(HDR_PARAMS_STR_LEN + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate params_str\n");
        return AVERROR(ENOMEM);
      }

      snprintf(*params_str, HDR_PARAMS_STR_LEN,
        "master-display=%s:max-cll=%s:", hdr_ctx->mdm_str, hdr_ctx->cll_str);
    }
  }

  return 0;
}

void hdr_ctx_free(HdrMetadataContext *hdr_ctx)
{
  free(hdr_ctx->mdm_str);
  free(hdr_ctx->cll_str);
  free(hdr_ctx);
}
