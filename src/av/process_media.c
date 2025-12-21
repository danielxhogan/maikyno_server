#include "process_media.h"

static int encode_video_frame(InputContext *in_ctx, OutputContext *out_ctx,
  int in_stream_idx, int out_stream_idx)
{
  int ret;

  if ((ret = avcodec_send_frame(
    out_ctx->enc_ctx[out_stream_idx], in_ctx->dec_frame)) < 0)
  {
    fprintf(stderr, "Failed to send frame to encoder.\n");
    return ret;
  }

  while ((ret = avcodec_receive_packet(
    out_ctx->enc_ctx[out_stream_idx], out_ctx->enc_pkt)) >= 0)
  {
    out_ctx->enc_pkt->stream_index = out_stream_idx;

    av_packet_rescale_ts(out_ctx->enc_pkt,
      in_ctx->fmt_ctx->streams[in_stream_idx]->time_base,
      out_ctx->fmt_ctx->streams[out_stream_idx]->time_base);

    if ((ret = av_interleaved_write_frame(out_ctx->fmt_ctx, out_ctx->enc_pkt)) < 0) {
      fprintf(stderr, "Failed to write packet to file.\n");
      return ret;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to receive packet from encoder.\n");
    return ret;
  }

  return 0;
}

int encode_audio_frame(OutputContext *out_ctx,
  int out_stream_idx, AVFrame *frame)
{
  int ret = 0;

  if ((ret = avcodec_send_frame(out_ctx->enc_ctx[out_stream_idx], frame)) < 0)
  {
    fprintf(stderr, "Failed to send frame to encoder.\nError: %s.\n",
      av_err2str(ret));
    return ret;
  }

  while ((ret = avcodec_receive_packet(
    out_ctx->enc_ctx[out_stream_idx], out_ctx->enc_pkt)) >= 0)
  {
    out_ctx->enc_pkt->stream_index = out_stream_idx;

    av_packet_rescale_ts(out_ctx->enc_pkt,
      (AVRational) {1, out_ctx->enc_ctx[out_stream_idx]->sample_rate},
      out_ctx->fmt_ctx->streams[out_stream_idx]->time_base);

    if ((ret =
      av_interleaved_write_frame(out_ctx->fmt_ctx, out_ctx->enc_pkt)) < 0)
    {
      fprintf(stderr, "Failed to write packet to file.\nError: %s.\n",
        av_err2str(ret));
      return ret;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to receive packet from encoder.\nError: %s.\n",
      av_err2str(ret));
    return ret;
  }

  return 0;
}

static int resample_audio_frame(InputContext *in_ctx, OutputContext *out_ctx,
  int out_stream_idx)
{
  int ret, nb_converted_samples;

  AVFrame *swr_frame = out_ctx->swr_frame[out_stream_idx];
  FrameSizeConversionContext *fsc_ctx = out_ctx->fsc_ctx[out_stream_idx];

  if ((ret = av_frame_make_writable(swr_frame)) < 0) {
    fprintf(stderr, "Failed to make frame writable.\nError: %s.\n", av_err2str(ret));
    return ret;
  }

  if ((ret = nb_converted_samples =
    swr_convert(out_ctx->swr_ctx[out_stream_idx], swr_frame->data,
      swr_frame->nb_samples, (const uint8_t **) in_ctx->dec_frame->data,
      in_ctx->dec_frame->nb_samples)) < 0)
  {
    fprintf(stderr, "Failed to convert audio frame.\nError: %s.\n", av_err2str(ret));
    return ret;
  }

  if ((ret = fsc_ctx_add_samples_to_buffer(
    fsc_ctx, swr_frame, nb_converted_samples)) < 0)
  {
    fprintf(stderr, "Failed to add samples to buffer.\n");
    return ret;
  }

  while (fsc_ctx->nb_samples_in_buffer >=
    out_ctx->enc_ctx[out_stream_idx]->frame_size)
  {
    if ((ret = fsc_ctx_make_frame(fsc_ctx,
      out_ctx->nb_samples_encoded[out_stream_idx])) < 0)
    {
      fprintf(stderr, "Failed to make frame for encoder.\n");
      return ret;
    }

    out_ctx->nb_samples_encoded[out_stream_idx] +=
      out_ctx->enc_ctx[out_stream_idx]->frame_size;

    if ((ret = encode_audio_frame(out_ctx,
      out_stream_idx, fsc_ctx->frame)) < 0)
    {
      fprintf(stderr, "Failed to write audio frame.\n");
      return ret;
    }
  }

  return 0;
}

int decode_packet(InputContext *in_ctx, OutputContext *out_ctx,
  int in_stream_idx, int out_stream_idx)
{
  int ret = 0;
  enum AVMediaType codec_type =
      in_ctx->fmt_ctx->streams[in_ctx->pkt->stream_index]->codecpar->codec_type;

  if ((ret =
    avcodec_send_packet(in_ctx->dec_ctx[in_stream_idx], in_ctx->pkt)) < 0)
  {
    fprintf(stderr, "Failed to send packet from input stream: %d to decoder.\n\
      Error: %s.\n", in_stream_idx, av_err2str(ret));
    return ret;
  }

  while ((ret = avcodec_receive_frame(in_ctx->dec_ctx[in_stream_idx],
    in_ctx->dec_frame)) >= 0)
  {
    in_ctx->dec_frame->pict_type = AV_PICTURE_TYPE_NONE;

    if (codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if ((ret = encode_video_frame(in_ctx, out_ctx,
        in_stream_idx, out_stream_idx)) < 0)
      {
        fprintf(stderr, "Failed to encode video frame from input stream: %d.\n",
          in_stream_idx);
        return ret;
      }
    }
    else if (codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if ((ret = resample_audio_frame(in_ctx, out_ctx, out_stream_idx)) < 0)
      {
        fprintf(stderr, "Failed to encode audio frame from input stream: %d.\n\n",
          in_stream_idx);
        return ret;
      }
    }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to receive frame from input stream: %d \
      from decoder.\nError: %s.\n", in_ctx->pkt->stream_index, av_err2str(ret));
    return ret;
  }

  return 0;
}

int transcode(InputContext *in_ctx, OutputContext *out_ctx,
  const char *batch_id, char *process_job_id)
{
  int frame_count, ret = 0;
  while ((ret = av_read_frame(in_ctx->fmt_ctx, in_ctx->pkt)) >= 0)
  {
    int in_stream_idx = in_ctx->pkt->stream_index;
    int out_stream_idx = in_ctx->map[in_stream_idx];

    if (out_stream_idx == INACTIVE_STREAM) {
      av_packet_unref(in_ctx->pkt);
      continue;
    }

    frame_count += 1;
    if (frame_count % 500 == 0) {
      if (check_abort_status(batch_id) == ABORTED) {
        return ABORTED;
      }

      if (calculate_pct_complete(in_ctx, process_job_id) < 0) {
        fprintf(stderr, "Failed to calculate percent complete.\n");
      }
    }

    if (!in_ctx->dec_ctx[in_stream_idx]) {
      in_ctx->pkt->stream_index = out_stream_idx;

      if ((ret =
        av_interleaved_write_frame(out_ctx->fmt_ctx, in_ctx->pkt)) < 0)
      {
        fprintf(stderr, "Failed to write packet to file.\nError: %s.\n",
          av_err2str(ret));
        return ret;
      }
      continue;
    }

    if ((ret = decode_packet(in_ctx, out_ctx, in_stream_idx, out_stream_idx)) < 0) {
      fprintf(stderr, "Failed to decode packet.\n");
      return ret;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to read frame.\nError: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

int process_video(char *process_job_id, const char *batch_id)
{
  int ret;

  if ((ret = check_abort_status(batch_id)) == ABORTED) {
    goto update_status;
  }

  int in_stream_idx, out_stream_idx;
  InputContext *in_ctx = NULL;
  OutputContext *out_ctx = NULL;
  enum AVMediaType codec_type;
  sqlite3 *db;

  if (update_process_job_status(process_job_id, PROCESSING) < 0) {
    fprintf(stderr, "Failed to update process job status \
      for process job: %s\n", process_job_id);
  }

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    ret = -ret;
    goto update_status;
  }

  if (!(in_ctx = open_input(process_job_id, db))) {
    fprintf(stderr, "Failed to open input for process job: %s.\n",
      process_job_id);
    ret = -1;
    goto update_status;
  }

  if (!(out_ctx = open_output(in_ctx, process_job_id, db))) {
    fprintf(stderr, "Failed to open output for process job: %s.\n",
      process_job_id);
    ret = -1;
    goto update_status;
  }

  sqlite3_close(db);
  db = NULL;

  if ((ret = transcode(in_ctx, out_ctx, batch_id, process_job_id)) < 0) {
    fprintf(stderr, "Failed to transcode process job: %s.\n", process_job_id);
  }

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    out_stream_idx = in_ctx->map[in_stream_idx];

    if (in_ctx->dec_ctx[in_stream_idx]) {
      if (avcodec_send_packet(in_ctx->dec_ctx[in_stream_idx], NULL) < 0)
      {
        fprintf(stderr, "Failed to send packet to decoder.\n");
        continue;
      }

      while (avcodec_receive_frame(in_ctx->dec_ctx[in_stream_idx],
          in_ctx->dec_frame) >= 0)
      {
        codec_type = in_ctx->dec_ctx[in_stream_idx]->codec_type;

        if (codec_type == AVMEDIA_TYPE_VIDEO)
        {
          if (encode_video_frame(in_ctx, out_ctx,
            in_stream_idx, out_stream_idx) < 0)
          {
            fprintf(stderr,
              "Failed to encode video frame from input stream: %d.\n",
              in_stream_idx);
            break;
          }
        }
        else if (codec_type == AVMEDIA_TYPE_AUDIO)
        {
          if (encode_audio_frame(out_ctx, out_stream_idx, NULL) < 0) {
            fprintf(stderr,
              "Failed to encode audio frame from input stream: %d.\n",
              in_stream_idx);
            break;
          }
        }
      }
    }
  }

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    out_stream_idx = in_ctx->map[in_stream_idx];

    if (in_ctx->dec_ctx[in_stream_idx])
    {
      codec_type = out_ctx->enc_ctx[out_stream_idx]->codec_type;
      in_ctx->dec_frame = NULL;

      if (codec_type == AVMEDIA_TYPE_VIDEO)
      {
        if (encode_video_frame(in_ctx, out_ctx,
          in_stream_idx, out_stream_idx) < 0)
        {
          fprintf(stderr,
            "Failed to encode video frame from input stream: %d.\n",
            in_stream_idx);
        }
      }
      else if (codec_type == AVMEDIA_TYPE_AUDIO)
      {
        if (encode_audio_frame(out_ctx, out_stream_idx, NULL) < 0)
        {
          fprintf(stderr,
            "Failed to encode audio frame from input stream: %d.\n",
            in_stream_idx);
        }
      }
    }
  }

  if (av_write_trailer(out_ctx->fmt_ctx) < 0) {
    fprintf(stderr, "Failed to write trailer to file.\n");
  }

  if (ret != ABORTED) {
    printf("pct_complete: 100%%\n");

    if (update_pct_complete(100, process_job_id) < 0) {
      fprintf(stderr, "Failed to update pct_complete to 100%% for process_job: %s\n",
        process_job_id);
    }
  }

update_status:
  if (ret == ABORTED)
  {
    if (update_process_job_status(process_job_id, ABORTED) < 0) {
      fprintf(stderr, "Failed to update process job status \
        for process job: %s\n", process_job_id);
    }
  } else if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
  {
    if (update_process_job_status(process_job_id, FAILED) < 0) {
      fprintf(stderr, "Failed to update process job status \
        for process job: %s\n", process_job_id);
    }
  } else {
    if (update_process_job_status(process_job_id, COMPLETE) < 0) {
      fprintf(stderr, "Failed to update process job status \
        for process job: %s\n", process_job_id);
    }
  }

  sqlite3_close(db);
  close_input(in_ctx);
  close_output(out_ctx);

  if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    return ret;
  }

  return 0;
}

int process_media(const char *batch_id)
{
  int batch_size = 0, i, j, ret;
  char *process_job_id, **process_job_ids;

  sqlite3 *db = NULL;

  char *select_batch_process_jobs_query =
    "SELECT process_jobs.id, batches.batch_size \
    FROM process_jobs \
    JOIN batches \
    ON process_jobs.batch_id = batches.id \
    WHERE batch_id = ?;";
  sqlite3_stmt *select_batch_process_jobs_stmt = NULL;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, select_batch_process_jobs_query, -1,
    &select_batch_process_jobs_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select batch process jobs statement \
      for batch id: %s\nError: %s\n", batch_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_batch_process_jobs_stmt, 1, batch_id,
    -1, SQLITE_STATIC);

  i = 0;
  while ((ret = sqlite3_step(select_batch_process_jobs_stmt)) == SQLITE_ROW)
  {
    if (batch_size == 0) {
      if (!(batch_size =
        sqlite3_column_int(select_batch_process_jobs_stmt, 1)))
      {
        fprintf(stderr, "Failed to get batch_size column int for batch id: \
          %s\nError: %s\n", batch_id, sqlite3_errmsg(db));
        goto end;
      }

      if (!(process_job_ids = calloc(batch_size, sizeof(char *)))) {
        fprintf(stderr, "Failed to allocate process_job_ids for batch: %s\n",
          batch_id);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      for (j = 0; j < batch_size; j++) {
        if (!(process_job_ids[j] = calloc(LEN_UUID_STRING, sizeof(char))))
        {
          fprintf(stderr, "Failed to allocate process_job_id for batch: %s\n",
            batch_id);
          ret = AVERROR(ENOMEM);
          goto end;
        }
      }
    }

    if (!(process_job_id =
      (char *) sqlite3_column_text(select_batch_process_jobs_stmt, 0)))
    {
      fprintf(stderr, "Failed to get process job id column text for batch id: \
        %s\nError: %s\n", batch_id, sqlite3_errmsg(db));
      goto end;
    }

    memcpy(process_job_ids[i], process_job_id, LEN_UUID_STRING);
    i++;
  }

  if (ret != SQLITE_DONE) {
    fprintf(stderr, "Failed to step through select_batch_process_jobs_stmt \
      for batch_id: %s\nError: %s.\n", batch_id,sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_finalize(select_batch_process_jobs_stmt);
  select_batch_process_jobs_stmt = NULL;
  sqlite3_close(db);
  db = NULL;

  for (i = 0; i < batch_size; i++) {
    if ((ret = process_video(process_job_ids[i], batch_id)) < 0) {
      if (ret != ABORTED) {
        fprintf(stderr, "Failed to process video for process job: %s\n",
          process_job_ids[i]);
        goto end;
      }
    }
  }
  
end:
  sqlite3_finalize(select_batch_process_jobs_stmt);
  sqlite3_close(db);
  return ret;
}
