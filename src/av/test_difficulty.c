#include "test_difficulty.h"

int encode_test_clip(char *in_clip_path, char *out_clip_path, int crf)
{
  char enc_params[1024];
  int ret = 0, v_stream_idx = -1;
  HdrMetadataContext *hdr_ctx = NULL;
  AVFormatContext *in_fmt_ctx = NULL, *out_fmt_ctx = NULL;
  AVStream *in_stream = NULL, *out_stream = NULL;
  const AVCodec *dec, *enc;
  AVCodecContext *dec_ctx = NULL, *enc_ctx = NULL;
  AVPacket *pkt = NULL;
  AVFrame *frame = NULL;

  #define ENCODER "libsvtav1"
  #define ENCODER_PARAMS "svtav1-params"

  int total_cores = get_core_count();
  int pin = total_cores / 3;
  int lp = pin / 2;
  if (lp <= 0) lp = 1;
  if (lp > 6) lp = 6;

  if (!(hdr_ctx = hdr_ctx_alloc())) {
    fprintf(stderr, "Failed to allocate HdrMetadataContext.\n");
    ret = -ENOMEM;
    goto end;
  }

  if ((ret = extract_hdr_metadata(hdr_ctx, in_clip_path)) < 0) {
    fprintf(stderr, "Failed to get hdr metatdata from input file.\n");
    goto end;
  }

  if ((ret = avformat_open_input(&in_fmt_ctx, in_clip_path, NULL, NULL)) < 0) {
    fprintf(stderr, "Failed to open input video file: '%s'.\n", in_clip_path);
    goto end;
  }

  if ((ret = avformat_find_stream_info(in_fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info.");
    goto end;
  }

  if ((ret = v_stream_idx =
    av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0)) < 0)
  {
    fprintf(stderr, "Failed to find video stream in input file.\n");
    goto end;
  }
  in_stream = in_fmt_ctx->streams[v_stream_idx];

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

  if (!(enc = avcodec_find_encoder_by_name(ENCODER))) {
    fprintf(stderr, "Failed to find encoder.\n");
    ret = AVERROR_UNKNOWN;
    goto end;
  }

  if (!(enc_ctx = avcodec_alloc_context3(enc))) {
    fprintf(stderr, "Failed to allocate encoder context.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  enc_ctx->time_base = in_stream->time_base;
  enc_ctx->framerate = av_guess_frame_rate(in_fmt_ctx, in_stream, NULL);

  enc_ctx->width = in_stream->codecpar->width;
  enc_ctx->height = in_stream->codecpar->height;
  enc_ctx->pix_fmt = in_stream->codecpar->format;

  enc_ctx->color_primaries = in_stream->codecpar->color_primaries;
  enc_ctx->color_trc = in_stream->codecpar->color_trc;
  enc_ctx->colorspace = in_stream->codecpar->color_space;
  enc_ctx->color_range = in_stream->codecpar->color_range;
  enc_ctx->chroma_sample_location = in_stream->codecpar->chroma_location;

  enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  sprintf(enc_params, "pin=%d:lp=%d:\
    crf=%d:preset=8:tune=0:\
    keyint=120:irefresh-type=2:scd=0:\
    tile-rows=2:tile-columns=2:fast-decode=2",
    pin, lp, crf);

  if ((ret = av_opt_set(enc_ctx->priv_data,
    ENCODER_PARAMS, enc_params, 0)) < 0)
  {
    fprintf(stderr, "Failed to set encoder params\n");
    goto end;
  }

  // if ((ret = inject_hdr_metadta(hdr_ctx, enc_ctx)) < 0) {
  //   fprintf(stderr, "Failed to inject hdr metadata.\n");
  //   goto end;
  // }

  if ((ret = avcodec_open2(enc_ctx, enc, NULL)) < 0) {
    fprintf(stderr, "Failed to open encoder.\n");
    goto end;
  }

  if ((ret =
    avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, out_clip_path)))
  {
    fprintf(stderr, "Failed to allocate output format context.\n");
    goto end;
  }

  if ((ret = av_dict_copy(&out_fmt_ctx->metadata,
    in_fmt_ctx->metadata, AV_DICT_DONT_OVERWRITE)) < 0)
  {
    fprintf(stderr, "Failed to copy file metadata.\n");
    goto end;
  }

  if (!(out_stream = avformat_new_stream(out_fmt_ctx, NULL))) {
    fprintf(stderr, "Failed to allocate new output stream.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if ((ret = av_dict_copy(&out_stream->metadata,
    in_stream->metadata, AV_DICT_DONT_OVERWRITE)) < 0)
  {
    fprintf(stderr, "Failed to copy video metadata.\n");
    goto end;
  }

  if ((ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx))) {
    fprintf(stderr,
      "Failed to copy codec parameters from encoder context to stream.\n");
    goto end;
  }

  out_stream->time_base = enc_ctx->time_base;
  out_stream->r_frame_rate = in_stream->r_frame_rate;
  out_stream->avg_frame_rate = in_stream->avg_frame_rate;

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

  if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    if ((ret = avio_open(&out_fmt_ctx->pb, out_clip_path, AVIO_FLAG_WRITE)) < 0) {
      fprintf(stderr, "Failed to open output file.\n");
      goto end;
    }
  }

  if ((ret = avformat_write_header(out_fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to write header for output file.\n");
    goto end;
  }

  while ((ret = av_read_frame(in_fmt_ctx, pkt)) >= 0)
  {
    if (!(pkt->stream_index == v_stream_idx)) {
      av_packet_unref(pkt);
      continue;
    }

    if ((ret = avcodec_send_packet(dec_ctx, pkt)) < 0) {
      fprintf(stderr, "Failed to send packet to decoder.\n");
      goto end;
    }

    while ((ret = avcodec_receive_frame(dec_ctx, frame)) >= 0)
    {
      if ((ret = avcodec_send_frame(enc_ctx, frame)) < 0) {
        fprintf(stderr, "Failed to send frame to encoder.\n");
        goto end;
      }

      while ((ret = avcodec_receive_packet(enc_ctx, pkt)) >= 0)
      {
        pkt->stream_index = 0;
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);

        if ((ret = av_interleaved_write_frame(out_fmt_ctx, pkt)) < 0) {
          fprintf(stderr, "Failed to write packet to file.\n");
          goto end;
        }
      }

      if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
        fprintf(stderr, "Failed to receive packet from encoder.\n");
        goto end;
      }
    }

    if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
      fprintf(stderr, "Failed to receive frame from decoder.\n");
      goto end;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to read frame from input file.\n");
    goto end;
  }

  if ((ret = avcodec_send_packet(dec_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to send packet to decoder.\n");
    goto end;
  }

  while ((ret = avcodec_receive_frame(dec_ctx, frame)) >= 0)
  {
    if ((ret = avcodec_send_frame(enc_ctx, frame)) < 0) {
      fprintf(stderr, "Failed to send frame to encoder.\n");
      goto end;
    }

    while ((ret = avcodec_receive_packet(enc_ctx, pkt)) >= 0)
    {
      pkt->stream_index = 0;
      av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);

      if ((ret = av_interleaved_write_frame(out_fmt_ctx, pkt)) < 0) {
        fprintf(stderr, "Failed to write packet to file.\n");
        goto end;
      }
    }

    if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
      fprintf(stderr, "Failed to receive packet from encoder.\n");
      goto end;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to receive frame from decoder.\n");
    goto end;
  }

  if ((ret = avcodec_send_frame(enc_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to send frame to encoder.\n");
    goto end;
  }

  while ((ret = avcodec_receive_packet(enc_ctx, pkt)) >= 0)
  {
    pkt->stream_index = 0;
    av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);

    if ((ret = av_interleaved_write_frame(out_fmt_ctx, pkt)) < 0) {
      fprintf(stderr, "Failed to write packet to file.\n");
      goto end;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to receive packet from encoder.\n");
    goto end;
  }

  if ((ret = av_write_trailer(out_fmt_ctx)) < 0) {
    fprintf(stderr, "Failed to write trailer to file.\n");
    goto end;
  }

end:
  free(hdr_ctx);
  avformat_close_input(&in_fmt_ctx);
  if (out_fmt_ctx && !(out_fmt_ctx->flags & AVFMT_NOFILE))
    avio_closep(&out_fmt_ctx->pb);
  avformat_free_context(out_fmt_ctx);
  avcodec_free_context(&dec_ctx);
  avcodec_free_context(&enc_ctx);
  av_packet_free(&pkt);
  av_frame_free(&frame);

  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "\nLibav Error: %s\n", av_err2str(ret));
    return -1;
  }

  return 0;
}

int64_t get_bitrate(char *clip_path)
{
  int64_t ret = 0;
  AVFormatContext *fmt_ctx = NULL;

  if ((ret = avformat_open_input(&fmt_ctx, clip_path, NULL, NULL)) < 0) {
    fprintf(stderr, "Failed to open input video file: '%s'.\n", clip_path);
    goto end;
  }

  if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info for %s\n", clip_path);
    goto end;
  }

  ret = fmt_ctx->bit_rate;

end:
  avformat_close_input(&fmt_ctx);
  return ret;
}

int64_t test_difficulty(const char *clips_dir_path,
  const char *test_diff_dir_path, const char *process_job_id)
{
  int current_crf, crf_nb_samples = 0, nb_samples = 0, ret = 0;
  int64_t bitrate, total_crf_bitrate = 0, average_crf_bitrate = 0,
    total_bitrate = 0, average_bitrate = 0;

  const char *end;
  size_t len_clips_dir_path, len_test_diff_dir_path, len_clip_filename,
    len_output_clips_dir_path;
  char current_crf_char[LEN_CRF + 1], *output_clips_dir_path = NULL,
    *in_clip_path = NULL, *out_clip_path = NULL;

  DIR *clips_dir;
  struct dirent *clip;

  sqlite3 *db = NULL;
  sqlite3_stmt *update_video_process_job_crf_bitrate_stmt = NULL;
  sqlite3_stmt *update_video_process_job_avg_bitrate_stmt = NULL;
  char update_video_process_job_crf_bitrate_query[1024];
  char *update_video_process_job_avg_bitrate_query;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    if (ret > 0) ret = -ret;
    goto end;
  }

  if ((ret = check_abort_status(process_job_id)) < 0) {
    goto end;
  }

  for (end = clips_dir_path; *end; end++);
  len_clips_dir_path = end - clips_dir_path;

  for (end = test_diff_dir_path; *end; end++);
  len_test_diff_dir_path = end - test_diff_dir_path;


  for (
    current_crf = STARTING_CRF;
    current_crf <= ENDING_CRF;
    current_crf += CRF_STEP)
  {
    if (!(clips_dir = opendir(clips_dir_path))) {
      fprintf(stderr, "Failed to open clips_dir_path: %s.\n", clips_dir_path);
      ret = -1;
      goto end_crf_loop;
    }

    snprintf(current_crf_char, LEN_CRF + 1, "%d", current_crf);

    len_output_clips_dir_path = len_test_diff_dir_path + LEN_CRF + 1;

    if (!(output_clips_dir_path =
      calloc(len_output_clips_dir_path + 1, sizeof(char))))
    {
      fprintf(stderr,
        "Failed to allocate output clips dir path for crf: %d\n", current_crf);
      ret = -ENOMEM;
      goto end_crf_loop;
    }

    strncat(output_clips_dir_path, test_diff_dir_path, len_test_diff_dir_path);
    strcat(output_clips_dir_path, "/");
    strncat(output_clips_dir_path, current_crf_char, LEN_CRF);

    while ((clip = readdir(clips_dir)) != NULL)
    {
      if ((ret = check_abort_status(process_job_id)) < 0) {
        goto end_readdir;
      }

      if (clip->d_type == DT_DIR) { continue; }
      crf_nb_samples++;
      nb_samples++;

      for (end = clip->d_name; *end; end++);
      len_clip_filename = end - clip->d_name;

      if (!(in_clip_path =
        calloc(len_clips_dir_path + len_clip_filename + 2, sizeof(char))))
      {
        fprintf(stderr,
          "Failed to allocate clip path for crf: %d, clip: %s\n",
          current_crf, clip->d_name);
        ret = -ENOMEM;
        goto end_readdir;
      }

      strncat(in_clip_path, clips_dir_path, len_clips_dir_path);
      strcat(in_clip_path, "/");
      strncat(in_clip_path, clip->d_name, len_clip_filename);

      if (!(out_clip_path =
        calloc(len_output_clips_dir_path + len_clip_filename + 2,
          sizeof(char))))
      {
        fprintf(stderr,
          "Failed to allocate output clip path for crf: %d, clip: %s\n",
          current_crf, clip->d_name);
        ret = -ENOMEM;
        goto end_readdir;
      }

      strncat(out_clip_path, output_clips_dir_path,
        len_output_clips_dir_path);
      strcat(out_clip_path, "/");
      strncat(out_clip_path, clip->d_name, len_clip_filename);

      if ((ret =
        encode_test_clip(in_clip_path, out_clip_path, current_crf)) < 0)
      {
        fprintf(stderr, "Failed to encoded test clip: %s\n", in_clip_path);
        goto end_readdir;
      }

      if ((ret = bitrate = get_bitrate(out_clip_path)) < 0) {
        fprintf(stderr, "Failed to get bitrate for %s\n", out_clip_path);
        goto end_readdir;
      }
      printf("bitrate: %ld\n", bitrate);

      total_bitrate += bitrate;
      total_crf_bitrate += bitrate;

end_readdir:
      free(in_clip_path);
      in_clip_path = NULL;

      free(out_clip_path);
      out_clip_path = NULL;

      if (ret < 0) goto end_crf_loop;
    }

    average_crf_bitrate = total_crf_bitrate / crf_nb_samples;

    sprintf(update_video_process_job_crf_bitrate_query,
      "UPDATE process_jobs SET avg_bitrate_crf%d = ? WHERE id = ?;", current_crf);

    if ((ret = sqlite3_prepare_v2(db, update_video_process_job_crf_bitrate_query,
      -1, &update_video_process_job_crf_bitrate_stmt, 0)) != SQLITE_OK)
    {
      fprintf(stderr, "Failed to prepare update crf bitrate statement.\nError: %s\n",
        sqlite3_errmsg(db));
      if (ret > 0) ret = -ret;
      goto end;
    }

    sqlite3_bind_int(update_video_process_job_crf_bitrate_stmt, 1,
      average_crf_bitrate);
    sqlite3_bind_text(update_video_process_job_crf_bitrate_stmt, 2, process_job_id,
      -1, SQLITE_STATIC);

    if ((ret =
      sqlite3_step(update_video_process_job_crf_bitrate_stmt)) != SQLITE_DONE)
    {
      fprintf(stderr,
        "Failed to update crf bitrate for process job: %s crf: %d\nError: %s\n",
        process_job_id, current_crf, sqlite3_errmsg(db));
      if (ret > 0) ret = -ret;
      goto end_crf_loop;
    }

    average_crf_bitrate = 0;
    total_crf_bitrate = 0;
    crf_nb_samples = 0;

end_crf_loop:
    closedir(clips_dir);
    free(output_clips_dir_path);

    if (ret < 0) goto end;
  }

  average_bitrate = total_bitrate / nb_samples;
  printf("average bitrate: %ld\n", average_bitrate);

  update_video_process_job_avg_bitrate_query =
    "UPDATE process_jobs SET avg_bitrate = ? WHERE id = ?;";

  if ((ret = sqlite3_prepare_v2(db, update_video_process_job_avg_bitrate_query,
    -1, &update_video_process_job_avg_bitrate_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare update avg bitrate statement. \
      \nError: %s\n", sqlite3_errmsg(db));
    if (ret > 0) ret = -ret;
    goto end;
  }

  sqlite3_bind_int(update_video_process_job_avg_bitrate_stmt, 1, average_bitrate);
  sqlite3_bind_text(update_video_process_job_avg_bitrate_stmt, 2, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret =
    sqlite3_step(update_video_process_job_avg_bitrate_stmt)) != SQLITE_DONE)
  {
    fprintf(stderr,
      "Failed to update average bitarte for process job: %s\nError: %s\n",
      process_job_id, sqlite3_errmsg(db));
    if (ret > 0) ret = -ret;
    goto end;
  }

end:
  sqlite3_close(db);
  sqlite3_finalize(update_video_process_job_crf_bitrate_stmt);
  sqlite3_finalize(update_video_process_job_avg_bitrate_stmt);

  if (ret < 0) return ret;
  return average_bitrate;
}
