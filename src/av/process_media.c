#include "process_media.h"

int encode_video_frame(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, AVFrame *frame, int out_stream_idx)
{
  int ret;
  int in_stream_idx = proc_ctx->v_stream_idx;

  if ((ret = avcodec_send_frame(
    out_ctx->enc_ctx[out_stream_idx], frame)) < 0)
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

    if (proc_ctx->deint) {
      out_ctx->enc_pkt->pts = out_ctx->enc_pkt->pts / 2;
      out_ctx->enc_pkt->dts = out_ctx->enc_pkt->dts / 2;
    }

    if ((ret = av_interleaved_write_frame(out_ctx->fmt_ctx,
      out_ctx->enc_pkt)) < 0)
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

int make_rendtion(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, int ctx_idx, AVFrame *frame)
{
  int ret = 0;
  RenditionFilterContext *rend_ctx = proc_ctx->rend_ctx_arr[ctx_idx];

  if ((ret = av_buffersrc_add_frame_flags(rend_ctx->buffersrc_ctx,
    frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to buffer source.\n");
    return ret;
  }

  while ((ret = av_buffersink_get_frame(rend_ctx->buffersink_ctx1,
    rend_ctx->filtered_frame1)) >= 0)
  {
    if ((ret = encode_video_frame(proc_ctx, in_ctx, out_ctx,
      rend_ctx->filtered_frame1, 0)) < 0)
    {
      fprintf(stderr, "Failed to encode video frame.\n");
      return ret;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to get frame from first buffer sink.\n");
    return ret;
  }

  while ((ret = av_buffersink_get_frame(rend_ctx->buffersink_ctx2,
    rend_ctx->filtered_frame2)) >= 0)
  {
    if ((ret = encode_video_frame(proc_ctx, in_ctx, out_ctx,
      rend_ctx->filtered_frame2, 1)) < 0)
    {
      fprintf(stderr, "Failed to encode video frame.\n");
      return ret;
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to get frame from first buffer sink.\n");
    return ret;
  }

  return 0;
}

int burn_in_subtitles(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, AVFilterContext *buffersrc_ctx, AVFrame *frame)
{
  int ret = 0;

  if ((ret = av_buffersrc_add_frame_flags(buffersrc_ctx,
    frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to buffer source.\n");
    return ret;
  }

  while ((ret = av_buffersink_get_frame(proc_ctx->burn_in_ctx->buffersink_ctx,
    proc_ctx->burn_in_ctx->filtered_frame)) >= 0)
  {
    if (proc_ctx->rend_ctx_arr[0]) {
      if ((ret = make_rendtion(proc_ctx, in_ctx, out_ctx,
        0, proc_ctx->burn_in_ctx->filtered_frame)) < 0)
      {
        fprintf(stderr, "Failed to make video renditions.\n");
        return ret;
      }
    }
    else {
      if ((ret = encode_video_frame(proc_ctx, in_ctx, out_ctx,
        proc_ctx->burn_in_ctx->filtered_frame, 0)) < 0)
      {
        fprintf(stderr, "Failed to encode video frame.\n");
        return ret;
      }
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to get frame from buffer sink.\n");
    return ret;
  }

  return 0;
}

int deinterlace_video_frame(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx)
{
  int ret = 0;

  if ((ret = av_buffersrc_add_frame_flags(proc_ctx->deint_ctx->buffersrc_ctx,
    in_ctx->dec_frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to buffer source.\n");
    return ret;
  }

  while ((ret = av_buffersink_get_frame(proc_ctx->deint_ctx->buffersink_ctx,
    proc_ctx->deint_ctx->filtered_frame)) >= 0)
  {
    if (proc_ctx->burn_in_ctx)
    {
      if ((ret = burn_in_subtitles(proc_ctx, in_ctx, out_ctx,
        proc_ctx->burn_in_ctx->v_buffersrc_ctx,
        proc_ctx->deint_ctx->filtered_frame)) < 0)
      {
        fprintf(stderr, "Failed to burn in subtitles.\n");
        return ret;
      }
    }
    else if (proc_ctx->rend_ctx_arr[0])
    {
      if ((ret = make_rendtion(proc_ctx, in_ctx, out_ctx,
        0, proc_ctx->deint_ctx->filtered_frame)) < 0)
      {
        fprintf(stderr, "Failed to make video renditions.\n");
        return ret;
      }
    }
    else {
      if ((ret = encode_video_frame(proc_ctx, in_ctx, out_ctx,
        proc_ctx->deint_ctx->filtered_frame, 0)) < 0)
      {
        fprintf(stderr, "Failed to encode video frame.\n");
        return ret;
      }
    }
  }

  if ((ret != AVERROR(EAGAIN)) && (ret != AVERROR_EOF)) {
    fprintf(stderr, "Failed to get frame from buffer sink.\n");
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

static int convert_audio_frame(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, int ctx_idx, int out_stream_idx)
{
  int ret, nb_converted_samples;
  SwrOutputContext *swr_out_ctx = proc_ctx->swr_out_ctx_arr[ctx_idx];
  FrameSizeConversionContext *fsc_ctx = proc_ctx->fsc_ctx_arr[ctx_idx];

  if (!in_ctx->dec_frame) {
    goto flush;
  }

  if ((ret = av_frame_make_writable(swr_out_ctx->swr_frame)) < 0) {
    fprintf(stderr, "Failed to make frame writable.\nError: %s.\n",
      av_err2str(ret));
    return ret;
  }

  if ((ret = nb_converted_samples = swr_convert(swr_out_ctx->swr_ctx,
    swr_out_ctx->swr_frame->data,
    swr_out_ctx->swr_frame->nb_samples,
    (const uint8_t **) in_ctx->dec_frame->data,
    in_ctx->dec_frame->nb_samples)) < 0)
  {
    fprintf(stderr, "Failed to convert audio frame.\nError: %s.\n",
      av_err2str(ret));
    return ret;
  }

  if ((ret = fsc_ctx_add_samples_to_buffer(
    fsc_ctx, swr_out_ctx->swr_frame, nb_converted_samples)) < 0)
  {
    fprintf(stderr, "Failed to add samples to buffer.\n");
    return ret;
  }

flush:
  while (fsc_ctx->nb_samples_in_buffer >=
    out_ctx->enc_ctx[out_stream_idx]->frame_size || !in_ctx->dec_frame)
  {
    if ((ret = fsc_ctx_make_frame(fsc_ctx,
      swr_out_ctx->nb_converted_samples)) < 0)
    {
      fprintf(stderr, "Failed to make frame for encoder.\n");
      return ret;
    }

    swr_out_ctx->nb_converted_samples +=
      out_ctx->enc_ctx[out_stream_idx]->frame_size;

    if ((ret = encode_audio_frame(out_ctx,
      out_stream_idx, fsc_ctx->frame)) < 0)
    {
      fprintf(stderr, "Failed to write audio frame.\n");
      return ret;
    }

    if (!in_ctx->dec_frame) { break; }
  }

  return 0;
}

int decode_sub_packet(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, int ctx_idx)
{
  int got_sub_ptr, ret = 0;
  AVCodecContext *s_dec_ctx = in_ctx->dec_ctx[ctx_idx];

  if ((ret = avcodec_decode_subtitle2(s_dec_ctx, in_ctx->dec_sub,
    &got_sub_ptr, in_ctx->init_pkt)) < 0)
  {
    fprintf(stderr, "Failed to decode subtitle.\n");
    return ret;
  }

  in_ctx->dec_sub->pts = in_ctx->init_pkt->pts;

  if ((ret = sub_to_frame_convert(proc_ctx->burn_in_ctx->stf_ctx, in_ctx)))
  {
    fprintf(stderr, "Failed to convert subtitle to frame.\n");
    return ret;
  }

  if ((ret = burn_in_subtitles(proc_ctx, in_ctx, out_ctx,
    proc_ctx->burn_in_ctx->s_buffersrc_ctx,
    proc_ctx->burn_in_ctx->stf_ctx->subtitle_frame)) < 0)
  {
    fprintf(stderr, "Failed to filter and encode frame.\n");
    return ret;
  }

  return 0;
}

int decode_av_packet(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, int in_stream_idx, int ctx_idx, int out_stream_idx)
{
  int ret = 0;
  enum AVMediaType codec_type = in_ctx->dec_ctx[ctx_idx]->codec_type;

  if ((ret =
    avcodec_send_packet(in_ctx->dec_ctx[ctx_idx], in_ctx->init_pkt)) < 0)
  {
    fprintf(stderr, "Failed to send packet from input stream: %d to decoder.\n\
      Error: %s.\n", in_stream_idx, av_err2str(ret));
    return ret;
  }

  while ((ret = avcodec_receive_frame(in_ctx->dec_ctx[ctx_idx],
    in_ctx->dec_frame)) >= 0)
  {
    in_ctx->dec_frame->pict_type = AV_PICTURE_TYPE_NONE;

    if (codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if (proc_ctx->deint) {
        if ((ret = deinterlace_video_frame(proc_ctx, in_ctx, out_ctx)) < 0) {
          fprintf(stderr, "Failed to deinterlace video frame from input stream: %d.\n",
            in_stream_idx);
          return ret;
        }
      }
      else if (proc_ctx->burn_in_ctx) {
        if ((ret = burn_in_subtitles(proc_ctx, in_ctx, out_ctx,
          proc_ctx->burn_in_ctx->v_buffersrc_ctx,
          in_ctx->dec_frame)) < 0)
        {
          fprintf(stderr, "Failed to burn in subtitles.\n");
          return ret;
        }
      }
      else if (proc_ctx->rend_ctx_arr[ctx_idx]) {
        if ((ret = make_rendtion(proc_ctx, in_ctx, out_ctx,
          ctx_idx, in_ctx->dec_frame)) < 0)
        {
          fprintf(stderr, "Failed to make video renditions.\n");
          return ret;
        }
      }
      else {
        if ((ret = encode_video_frame(proc_ctx, in_ctx, out_ctx,
          in_ctx->dec_frame, 0)) < 0)
        {
          fprintf(stderr, "Failed to encode video frame from input stream: %d.\n",
            in_stream_idx);
          return ret;
        }
      }
    }
    else if (codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if ((ret = convert_audio_frame(proc_ctx, in_ctx,
        out_ctx, ctx_idx, out_stream_idx)) < 0)
      {
        fprintf(stderr, "Failed to encode audio frame from input stream: %d.\n\n",
          in_stream_idx);
        return ret;
      }
    }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to receive frame from input stream: %d \
      from decoder.\nError: %s.\n", in_ctx->init_pkt->stream_index, av_err2str(ret));
    return ret;
  }

  return 0;
}

int decode_packet(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, int in_stream_idx, int ctx_idx, int out_stream_idx)
{
  int ret = 0;
  enum AVMediaType codec_type = in_ctx->dec_ctx[ctx_idx]->codec_type;

  if (codec_type == AVMEDIA_TYPE_VIDEO || codec_type == AVMEDIA_TYPE_AUDIO)
  {
    if ((ret = decode_av_packet(proc_ctx, in_ctx, out_ctx,
      in_stream_idx, ctx_idx, out_stream_idx)) < 0)
    {
      fprintf(stderr, "Failed to decode audio or video packet \
        for input stream: %d.\n", in_stream_idx);
      return ret;
    }
  } else if (codec_type == AVMEDIA_TYPE_SUBTITLE && in_ctx->init_pkt)
  {
    if ((ret = decode_sub_packet(proc_ctx, in_ctx, out_ctx, ctx_idx)) < 0) {
      fprintf(stderr, "Failed to subtitle packet \
        for input stream: %d.\n", in_stream_idx);
      return ret;
    }
  }

  return 0;
}

int transcode(ProcessingContext *proc_ctx, InputContext *in_ctx,
  OutputContext *out_ctx, const char *batch_id, char *process_job_id)
{
  int frame_count, ret = 0;
  while ((ret = av_read_frame(in_ctx->fmt_ctx, in_ctx->init_pkt)) >= 0)
  {
    int in_stream_idx = in_ctx->init_pkt->stream_index;
    int ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    int out_stream_idx = proc_ctx->idx_map[in_stream_idx];

    if (ctx_idx == INACTIVE_STREAM) {
      av_packet_unref(in_ctx->init_pkt);
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

    if (proc_ctx->passthrough_arr[ctx_idx]) {
      in_ctx->init_pkt->stream_index = out_stream_idx;

      if ((ret =
        av_interleaved_write_frame(out_ctx->fmt_ctx, in_ctx->init_pkt)) < 0)
      {
        fprintf(stderr, "Failed to write packet to file.\nError: %s.\n",
          av_err2str(ret));
        return ret;
      }
      continue;
    }

    if ((ret = decode_packet(proc_ctx, in_ctx, out_ctx,
      in_stream_idx, ctx_idx, out_stream_idx)) < 0)
    {
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
  int in_stream_idx, out_stream_idx, ctx_idx, ret;

  if ((ret = check_abort_status(batch_id)) == ABORTED) {
    goto update_status;
  }

  ProcessingContext *proc_ctx = NULL;
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

  if (!(proc_ctx = processing_context_alloc(process_job_id, db))) {
    fprintf(stderr, "Failed to allocate processing context for\
      process job: %s.\n", process_job_id);
    goto update_status;
  }

  if ((ret = get_processing_info(proc_ctx, process_job_id, db)) < 0) {
    fprintf(stderr, "Failed to get prcessing info for proces job: %s.\n",
      process_job_id);
    goto update_status;
  }

  if (!(in_ctx = open_input(proc_ctx, process_job_id, db))) {
    fprintf(stderr, "Failed to open input for process job: %s.\n",
      process_job_id);
    ret = -1;
    goto update_status;
  }

  if (!(out_ctx = open_output(proc_ctx, in_ctx, process_job_id, db))) {
    fprintf(stderr, "Failed to open output for process job: %s.\n",
      process_job_id);
    ret = -1;
    goto update_status;
  }


  if ((ret = processing_context_init(proc_ctx, in_ctx, out_ctx,
    process_job_id)) < 0)
  {
    fprintf(stderr, "Failed to initialize processing context.\n");
    goto update_status;
  }

  sqlite3_close(db);
  db = NULL;

  if ((ret = transcode(proc_ctx, in_ctx, out_ctx, batch_id, process_job_id)) < 0) {
    fprintf(stderr, "Failed to transcode process job: %s.\n", process_job_id);
  }

  in_ctx->init_pkt = NULL;

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    out_stream_idx = proc_ctx->idx_map[in_stream_idx];

    if (ctx_idx == INACTIVE_STREAM) { continue; }
    if (proc_ctx->passthrough_arr[ctx_idx]) { continue; }

    if ((ret = decode_packet(proc_ctx, in_ctx, out_ctx,
      in_stream_idx, ctx_idx, out_stream_idx)) < 0)
    {
      fprintf(stderr, "Failed to decode packet.\n");
      return ret;
    }
  }

  in_ctx->dec_frame = NULL;

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    out_stream_idx = proc_ctx->idx_map[in_stream_idx];

    if (ctx_idx == INACTIVE_STREAM) { continue; }
    if (proc_ctx->passthrough_arr[ctx_idx]) { continue; }
    if (in_ctx->dec_ctx[ctx_idx]->codec_type != AVMEDIA_TYPE_AUDIO) { continue; }

    if ((ret = convert_audio_frame(proc_ctx, in_ctx, out_ctx,
      ctx_idx, out_stream_idx)) < 0)
    {
      fprintf(stderr, "Failed to encode audio frame from input stream: %d.\n\n",
        in_stream_idx);
      return ret;
    }
  }

  if (proc_ctx->deint_ctx) {
    if (deinterlace_video_frame(proc_ctx, in_ctx, out_ctx) < 0) {
      fprintf(stderr, "Failed to flush deinterlace filter.\n");
    }
  }

  if (proc_ctx->burn_in_ctx) {
    if (burn_in_subtitles(proc_ctx, in_ctx, out_ctx,
      proc_ctx->burn_in_ctx->v_buffersrc_ctx, NULL) < 0) {
        fprintf(stderr, "Failed to flush burn in filter.\n");
    }

    if (burn_in_subtitles(proc_ctx, in_ctx, out_ctx,
      proc_ctx->burn_in_ctx->s_buffersrc_ctx, NULL) < 0) {
        fprintf(stderr, "Failed to flush burn in filter.\n");
    }
  }

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    out_stream_idx = proc_ctx->idx_map[in_stream_idx];

    if (ctx_idx == INACTIVE_STREAM) { continue; }
    if (proc_ctx->passthrough_arr[ctx_idx]) { continue; }

    codec_type = in_ctx->dec_ctx[ctx_idx]->codec_type;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if ((ret = encode_video_frame(proc_ctx, in_ctx, out_ctx,
        NULL, 0)) < 0)
      {
        fprintf(stderr, "Failed to encode video frame from input stream: %d.\n",
          in_stream_idx);
        return ret;
      }
    } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if ((ret = encode_audio_frame(out_ctx,
        out_stream_idx, NULL)) < 0)
      {
        fprintf(stderr, "Failed to write audio frame.\n");
        return ret;
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
  processing_context_free(&proc_ctx);
  close_input(&in_ctx);
  close_output(&out_ctx);


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
      for batch_id: %s\nError: %s.\n", batch_id, sqlite3_errmsg(db));
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
