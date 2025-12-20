#include "process_media.h"

static int encode_video_frame(OutputContext *out_ctx, int out_stream_idx,
  InputContext *in_ctx, int in_stream_idx)
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

int write_audio_frame(OutputContext *out_ctx, int out_stream_idx)
{
  int ret;

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

static int encode_audio_frame(OutputContext *out_ctx, int out_stream_idx,
  InputContext *in_ctx, int flush_enc)
{
  int ret, nb_converted_samples;

  if (flush_enc) {
    if ((ret =
      avcodec_send_frame(out_ctx->enc_ctx[out_stream_idx], in_ctx->dec_frame)) < 0)
    {
      fprintf(stderr, "Failed to send frame to encoder.\n");
      return ret;
    }

    if ((ret = write_audio_frame(out_ctx, out_stream_idx)) < 0) {
      fprintf(stderr, "Failed to write audio frame while flushing encoder.\n");
      return ret;
    }

    return 0;
  }

  AVFrame *swr_frame = out_ctx->swr_frame[out_stream_idx];
  FrameSizeConversionContext *fsc_ctx = out_ctx->fsc_ctx[out_stream_idx];

  if ((ret = av_frame_make_writable(swr_frame)) < 0) {
    fprintf(stderr, "Failed to make frame writable.\n");
    return ret;
  }

  if ((ret = nb_converted_samples =
    swr_convert(out_ctx->swr_ctx[out_stream_idx], swr_frame->data,
      swr_frame->nb_samples, (const uint8_t **) in_ctx->dec_frame->data,
      in_ctx->dec_frame->nb_samples)) < 0)
  {
    fprintf(stderr, "Failed to convert audio frame.\n");
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

    if ((ret =
      avcodec_send_frame(out_ctx->enc_ctx[out_stream_idx], fsc_ctx->frame)) < 0)
    {
      fprintf(stderr, "Failed to send frame to encoder.\n");
      return ret;
    }

    if ((ret = write_audio_frame(out_ctx, out_stream_idx)) < 0) {
      fprintf(stderr, "Failed to write audio frame.\n");
      return ret;
    }
  }

  return 0;
}

int update_process_job_status(char *process_job_id, enum ProcessJobStatus status)
{
  int ret = 0;
  sqlite3 *db;

  char *update_process_job_status_query =
    "UPDATE process_jobs \
    SET job_status = ? \
    WHERE id = ?;";
  sqlite3_stmt *update_process_job_status_stmt = NULL;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nError: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, update_process_job_status_query, -1,
    &update_process_job_status_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare update process job status statement. \
      \nError: %s\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(update_process_job_status_stmt, 1,
    job_status_enum_to_string(status), -1, SQLITE_STATIC);

  sqlite3_bind_text(update_process_job_status_stmt, 2, process_job_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(update_process_job_status_stmt)) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update status for process_job: \
      %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

end:
  sqlite3_finalize(update_process_job_status_stmt);
  sqlite3_close(db);
  return ret;
}

int process_video(char *process_job_id, const char *batch_id)
{
  int aborted = 0, ret;

  if ((ret = check_abort_status(batch_id)) < 0) {
    if (ret != ABORTED) {
      fprintf(stderr, "Failed to check abort status for process_job: %s\n",
        batch_id);
    }
    aborted = 1;
    goto update_status;
  }

  int in_stream_idx, out_stream_idx, frame_count = 0,
    status_checks_ldr = 0, status_check_flr = 0;

  int64_t duration = -1, pct_complete;

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

  while ((ret = av_read_frame(in_ctx->fmt_ctx, in_ctx->pkt)) >= 0)
  {
    in_stream_idx = in_ctx->pkt->stream_index;
    out_stream_idx = in_ctx->map[in_stream_idx];
    codec_type =
      in_ctx->fmt_ctx->streams[in_ctx->pkt->stream_index]->codecpar->codec_type;

    if (out_stream_idx == INACTIVE_STREAM) {
      av_packet_unref(in_ctx->pkt);
      continue;
    }

    frame_count += 1;

    if (frame_count % 500 == 0) {
      if ((ret = check_abort_status(batch_id)) < 0) {
        if (ret == ABORTED) {
          aborted = 1;
          goto flush;
        }
        else {
          fprintf(stderr, "Failed to check abort status for batch: %s\n",
            batch_id);
          goto flush;
        }
      }
    }

    if (frame_count % 500 == 0 || status_checks_ldr > status_check_flr) {
      if (frame_count % 500 == 0) { status_checks_ldr += 1; }

      if (codec_type == AVMEDIA_TYPE_VIDEO) {
        status_check_flr += 1;

        if (duration == -1) {
          duration = av_rescale_q(in_ctx->fmt_ctx->duration, AV_TIME_BASE_Q,
            in_ctx->fmt_ctx->streams[in_ctx->pkt->stream_index]->time_base);
        }

        pct_complete = in_ctx->pkt->pts * 100 / duration;
        printf("pct_complete: %ld%%\n", pct_complete);

        if ((ret = update_pct_complete(pct_complete, process_job_id)) < 0) {
          fprintf(stderr, "Failed to update pct_complete for process_job: %s\n",
            process_job_id);
        }
      }
    }

    if (!in_ctx->dec_ctx[in_stream_idx]) {
      in_ctx->pkt->stream_index = out_stream_idx;

      if ((ret =
        av_interleaved_write_frame(out_ctx->fmt_ctx, in_ctx->pkt)) < 0)
      {
        fprintf(stderr, "Failed to write packet to file.\n");
        goto flush;
      }
      continue;
    }

    if ((ret =
      avcodec_send_packet(in_ctx->dec_ctx[in_stream_idx],
        in_ctx->pkt)) < 0)
    {
      fprintf(stderr,
        "Failed to send packet from input stream: %d to decoder.\n",
        in_stream_idx);
      goto end;
    }

    while ((ret =
      avcodec_receive_frame(in_ctx->dec_ctx[in_stream_idx],
        in_ctx->dec_frame)) >= 0)
    {

      if (codec_type == AVMEDIA_TYPE_VIDEO)
      {
        if ((ret = encode_video_frame(out_ctx, out_stream_idx,
          in_ctx, in_stream_idx)) < 0)
        {
          fprintf(stderr,
            "Failed to encode video frame from input stream: %d.\n",
            in_stream_idx);
          goto flush;
        }
      }
      else if (codec_type == AVMEDIA_TYPE_AUDIO)
      {
        if ((ret = encode_audio_frame(out_ctx, out_stream_idx, in_ctx, 0)) < 0)
        {
          fprintf(stderr,
            "Failed to encode audio frame from input stream: %d.\n",
            in_stream_idx);
          goto flush;
        }
      }
    }

    if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
      fprintf(stderr,
        "Failed to receive frame from input stream: %d from decoder.\n",
        in_ctx->pkt->stream_index);
      goto flush;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to read frame.\n");
    goto flush;
  }

flush:
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
          if (encode_video_frame(out_ctx, out_stream_idx,
            in_ctx, in_stream_idx) < 0)
          {
            fprintf(stderr,
              "Failed to encode video frame from input stream: %d.\n",
              in_stream_idx);
            break;
          }
        }
        else if (codec_type == AVMEDIA_TYPE_AUDIO)
        {
          if (encode_audio_frame(out_ctx, out_stream_idx, in_ctx, 0) < 0) {
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
        if (encode_video_frame(out_ctx, out_stream_idx,
          in_ctx, in_stream_idx) < 0)
        {
          fprintf(stderr,
            "Failed to encode video frame from input stream: %d.\n",
            in_stream_idx);
        }
      }
      else if (codec_type == AVMEDIA_TYPE_AUDIO)
      {
        if (encode_audio_frame(out_ctx, out_stream_idx, in_ctx, 1) < 0)
        {
          fprintf(stderr,
            "Failed to encode audio frame from input stream: %d.\n",
            in_stream_idx);
        }
      }
    }
  }

  printf("pct_complete: 100%%\n");

  if (av_write_trailer(out_ctx->fmt_ctx) < 0) {
    fprintf(stderr, "Failed to write trailer to file.\n");
  }

  if (update_pct_complete(100, process_job_id) < 0) {
    fprintf(stderr, "Failed to update pct_complete to 100%% for process_job: %s\n",
      process_job_id);
  }

update_status:
  if (aborted) {
    if (update_process_job_status(process_job_id, ABORTED) < 0) {
      fprintf(stderr, "Failed to update process job status \
        for process job: %s\n", process_job_id);
    }
  }
  else if (ret < 0) {
    if (update_process_job_status(process_job_id, FAILED) < 0) {
      fprintf(stderr, "Failed to update process job status \
        for process job: %s\n", process_job_id);
    }
  }
  else {
    if (update_process_job_status(process_job_id, COMPLETE) < 0) {
      fprintf(stderr, "Failed to update process job status \
        for process job: %s\n", process_job_id);
    }
  }

fm:
  sqlite3_close(db);
  close_input(in_ctx);
  close_output(out_ctx);

  if (aborted) return ABORTED;
  if ((ret < 0 && ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
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
    ret = -ret;
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
