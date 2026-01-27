#include "process_media.h"

#include "proc.h"
#include "input.h"
#include "output.h"
#include "burn_in.h"
#include "utils.h"

int encode_video_frame(ProcessingContext *proc_ctx, StreamContext *stream_ctx,
  AVFrame *frame, int rendition)
{
  int out_stream_idx, ret = 0;
  AVCodecContext *enc_ctx;
  AVStream *out_stream;

  if (!rendition) {
    enc_ctx = stream_ctx->rend0_enc_ctx;
    out_stream = stream_ctx->rend0_out_stream;
    out_stream_idx = stream_ctx->rend0_out_stream_idx;
  }
  else {
    enc_ctx = stream_ctx->rend1_enc_ctx;
    out_stream = stream_ctx->rend1_out_stream;
    out_stream_idx = stream_ctx->rend1_out_stream_idx;
  }

  if ((ret = avcodec_send_frame( enc_ctx, frame)) < 0)
  {
    fprintf(stderr, "Failed to send video frame to encoder.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while ((ret = avcodec_receive_packet(enc_ctx, proc_ctx->pkt)) >= 0)
  {
    proc_ctx->pkt->stream_index = out_stream_idx;

    av_packet_rescale_ts(proc_ctx->pkt,
      stream_ctx->in_stream->time_base,
      out_stream->time_base);

    if (proc_ctx->deint) {
      proc_ctx->pkt->pts = proc_ctx->pkt->pts / 2;
      proc_ctx->pkt->dts = proc_ctx->pkt->dts / 2;
    }

    if ((ret = av_interleaved_write_frame(proc_ctx->out_fmt_ctx,
      proc_ctx->pkt)) < 0)
    {
      fprintf(stderr, "Failed to write video packet to stream '%d'.\n\
        Libav Error: %s.\n", out_stream_idx,av_err2str(ret));
      return ret;
    }

    if (proc_ctx->pkt) { av_packet_unref(proc_ctx->pkt); }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to receive video packet from encoder.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

int make_rendtion(ProcessingContext *proc_ctx, StreamContext *stream_ctx,
  AVFrame *frame)
{
  int ret = 0;
  RenditionFilterContext *rend_ctx = proc_ctx->rend_ctx;

  if ((ret = av_buffersrc_add_frame_flags(rend_ctx->buffersrc_ctx,
    frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to rendition buffer source.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while ((ret = av_buffersink_get_frame(rend_ctx->buffersink_ctx1,
    rend_ctx->filtered_frame1)) >= 0)
  {
    if ((ret = encode_video_frame(proc_ctx, stream_ctx,
      rend_ctx->filtered_frame1, 0)) < 0)
    {
      fprintf(stderr, "Failed to encode first rendition frame.\n");
      return ret;
    }

    av_frame_unref(rend_ctx->filtered_frame1);
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to get frame from first rendition buffer sink.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while ((ret = av_buffersink_get_frame(rend_ctx->buffersink_ctx2,
    rend_ctx->filtered_frame2)) >= 0)
  {
    if ((ret = encode_video_frame(proc_ctx, stream_ctx,
      rend_ctx->filtered_frame2, 1)) < 0)
    {
      fprintf(stderr, "Failed to encode second rendition frame.\n");
      return ret;
    }

    av_frame_unref(rend_ctx->filtered_frame2);
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to get frame from second rendition buffer sink.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

int burn_in_subtitles(ProcessingContext *proc_ctx, StreamContext *stream_ctx,
  AVFilterContext *buffersrc_ctx, AVFrame *frame)
{
  int ret = 0;
  AVCodecContext *v_dec_ctx = proc_ctx->stream_ctx_arr[0]->dec_ctx;
  AVFrame *filtered_frame;

  if ((ret = av_buffersrc_add_frame_flags(buffersrc_ctx,
    frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to burn in buffer source.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while(1)
  {
    ret = av_buffersink_get_frame(proc_ctx->burn_in_ctx->buffersink_ctx,
      proc_ctx->burn_in_ctx->filtered_frame);

    if (ret == AVERROR(EAGAIN)) {
      if (avfilter_graph_request_oldest(
        proc_ctx->burn_in_ctx->filter_graph) < 0) { break; }
      continue;
    }

    filtered_frame = proc_ctx->burn_in_ctx->filtered_frame;
    filtered_frame->width = v_dec_ctx->width;
    filtered_frame->height = v_dec_ctx->height;
    filtered_frame->format = v_dec_ctx->pix_fmt;
    filtered_frame->color_primaries = v_dec_ctx->color_primaries;
    filtered_frame->color_trc = v_dec_ctx->color_trc;
    filtered_frame->colorspace = v_dec_ctx->colorspace;
    filtered_frame->chroma_location = v_dec_ctx->chroma_sample_location;
    filtered_frame->color_range = v_dec_ctx->color_range;

    if (filtered_frame->pts <= 0) {
      filtered_frame->pts =
        (proc_ctx->tminus1_v_pts * 2) - proc_ctx->tminus2_v_pts;
    } else {
      proc_ctx->tminus2_v_pts = proc_ctx->tminus1_v_pts;
      proc_ctx->tminus1_v_pts = filtered_frame->pts;
    }

    if (proc_ctx->rend_ctx) {
      if ((ret = make_rendtion(proc_ctx, stream_ctx,
        filtered_frame)) < 0)
      {
        fprintf(stderr, "Failed to make renditions of burn in frame.\n");
        return ret;
      }
    }
    else {
      if ((ret = encode_video_frame(proc_ctx, stream_ctx,
        filtered_frame, 0)) < 0)
      {
        fprintf(stderr, "Failed to encode burn in frame.\n");
        return ret;
      }
    }

    av_frame_unref(filtered_frame);
    if (ret < 0) { break; }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to get frame from burn in buffer sink.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

int deinterlace_video_frame(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, InputContext *in_ctx)
{
  int ret = 0;

  if ((ret = av_buffersrc_add_frame_flags(proc_ctx->deint_ctx->buffersrc_ctx,
    in_ctx->dec_frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to deinterlace buffer source.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while ((ret = av_buffersink_get_frame(proc_ctx->deint_ctx->buffersink_ctx,
    proc_ctx->deint_ctx->filtered_frame)) >= 0)
  {
    if (proc_ctx->burn_in_ctx && proc_ctx->first_sub)
    {
      if (in_ctx->dec_frame->pts > proc_ctx->last_sub_pts + 5000) {
        push_dummy_subtitle(proc_ctx, stream_ctx, in_ctx->dec_frame->pts);
        proc_ctx->last_sub_pts = in_ctx->dec_frame->pts;
      }

      if ((ret = burn_in_subtitles(proc_ctx, stream_ctx,
        proc_ctx->burn_in_ctx->v_buffersrc_ctx,
        proc_ctx->deint_ctx->filtered_frame)) < 0)
      {
        fprintf(stderr, "Failed to burn in deinterlaced frame.\n");
        return ret;
      }
    }
    else if (proc_ctx->rend_ctx)
    {
      if ((ret = make_rendtion(proc_ctx, stream_ctx,
        proc_ctx->deint_ctx->filtered_frame)) < 0)
      {
        fprintf(stderr, "Failed to make renditions of deinterlaced frame.\n");
        return ret;
      }
    }
    else {
      if ((ret = encode_video_frame(proc_ctx, stream_ctx,
        proc_ctx->deint_ctx->filtered_frame, 0)) < 0)
      {
        fprintf(stderr, "Failed to encode deinterlaced frame.\n");
        return ret;
      }
    }

    av_frame_unref(proc_ctx->deint_ctx->filtered_frame);
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to get frame from deinterlace buffer sink.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

int encode_audio_frame(ProcessingContext *proc_ctx, StreamContext *stream_ctx,
  int rendition, AVFrame *frame)
{
  int out_stream_idx, ret = 0;
  AVCodecContext *enc_ctx;
  AVStream *out_stream;

  if (!rendition) {
    enc_ctx = stream_ctx->rend0_enc_ctx;
    out_stream_idx = stream_ctx->rend0_out_stream_idx;
    out_stream = stream_ctx->rend0_out_stream;
  }
  else {
    enc_ctx = stream_ctx->rend1_enc_ctx;
    out_stream_idx = stream_ctx->rend1_out_stream_idx;
    out_stream = stream_ctx->rend1_out_stream;
  }

  if ((ret = avcodec_send_frame(enc_ctx, frame)) < 0)
  {
    fprintf(stderr, "Failed to send audio frame to encoder.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while ((ret = avcodec_receive_packet( enc_ctx, proc_ctx->pkt)) >= 0) {
    proc_ctx->pkt->stream_index = out_stream_idx;

    av_packet_rescale_ts(proc_ctx->pkt,
      (AVRational) { 1, enc_ctx->sample_rate },
      out_stream->time_base);

    if ((ret =
      av_interleaved_write_frame(proc_ctx->out_fmt_ctx, proc_ctx->pkt)) < 0)
    {
      fprintf(stderr, "Failed to write audio packet to stream '%d'.\n\
        Libav Error: %s.\n", out_stream_idx, av_err2str(ret));
      return ret;
    }

    if (proc_ctx->pkt) { av_packet_unref(proc_ctx->pkt); }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to receive packet from encoder.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

int boost_gain(ProcessingContext *proc_ctx, StreamContext *stream_ctx,
  int rendition, AVFrame *frame)
{
  int out_stream_idx, ret = 0;

  if (!rendition) {
    out_stream_idx = stream_ctx->rend0_out_stream_idx;
  } else {
    out_stream_idx = stream_ctx->rend1_out_stream_idx;
  }

  if ((ret = av_buffersrc_add_frame_flags(proc_ctx->vol_ctx_arr[out_stream_idx]->buffersrc_ctx,
    frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
  {
    fprintf(stderr, "Failed to add frame to boost gain buffer source.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  while ((ret = av_buffersink_get_frame(proc_ctx->vol_ctx_arr[out_stream_idx]->buffersink_ctx,
    proc_ctx->vol_ctx_arr[out_stream_idx]->filtered_frame)) >= 0)
  {
    if ((ret = encode_audio_frame(proc_ctx, stream_ctx, rendition,
      proc_ctx->vol_ctx_arr[out_stream_idx]->filtered_frame)))
    {
      fprintf(stderr, "Failed to write audio frame after boosting gain.\n");
      return ret;
    }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to get frame from volume filter buffer sink.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}

static int convert_audio_frame(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, int rendition, AVFrame *frame)
{
  int out_stream_idx, nb_converted_samples,
    sample_rate, start_time, timestamp, ret = 0;
  AVCodecContext *enc_ctx;
  int64_t stream_start_time;
  AVRational stream_time_base, pts_time_base;

  if (!rendition) {
    enc_ctx = stream_ctx->rend0_enc_ctx;
    out_stream_idx = stream_ctx->rend0_out_stream_idx;
  }
  else {
    enc_ctx = stream_ctx->rend1_enc_ctx;
    out_stream_idx = stream_ctx->rend1_out_stream_idx;
  }

  SwrOutputContext *swr_out_ctx = proc_ctx->swr_out_ctx_arr[out_stream_idx];
  FrameSizeConversionContext *fsc_ctx = proc_ctx->fsc_ctx_arr[out_stream_idx];

  if (!frame) {
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
    (const uint8_t **) frame->data,
    frame->nb_samples)) < 0)
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
  stream_start_time = stream_ctx->in_stream->start_time;
  stream_time_base = stream_ctx->in_stream->time_base;

  sample_rate = enc_ctx->sample_rate;
  pts_time_base = (AVRational) {1, sample_rate};

  start_time = av_rescale_q(stream_start_time, stream_time_base, pts_time_base);

  while (fsc_ctx->nb_samples_in_buffer >=
    enc_ctx->frame_size || !frame)
  {
    timestamp = swr_out_ctx->nb_converted_samples + start_time;

    if ((ret = fsc_ctx_make_frame(fsc_ctx, timestamp)) < 0) {
      fprintf(stderr, "Failed to make frame for encoder.\n");
      return ret;
    }

    swr_out_ctx->nb_converted_samples +=
      enc_ctx->frame_size;

    if (proc_ctx->vol_ctx_arr[out_stream_idx])
    {
      if ((ret = boost_gain(proc_ctx, stream_ctx, rendition,
        fsc_ctx->frame)))
      {
        fprintf(stderr, "Failed to boost gain.\n");
        return ret;
      }
    } else {
      if ((ret = encode_audio_frame(proc_ctx, stream_ctx, rendition,
        fsc_ctx->frame)) < 0)
      {
        fprintf(stderr, "Failed to write audio frame.\n");
        return ret;
      }
    }

    if (!frame) { break; }
  }

  return 0;
}

int decode_sub_packet(ProcessingContext *proc_ctx, StreamContext *stream_ctx,
  InputContext *in_ctx)
{
  int got_sub_ptr, ret = 0;
  AVCodecContext *s_dec_ctx = stream_ctx->dec_ctx;

  if (proc_ctx->first_sub == 0) {
    proc_ctx->first_sub = 1;
    printf("Found first subtitle\n");
  }

  if ((ret = avcodec_decode_subtitle2(s_dec_ctx, in_ctx->dec_sub,
    &got_sub_ptr, in_ctx->init_pkt)) < 0)
  {
    fprintf(stderr, "Failed to decode subtitle.\n\
      Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  if (proc_ctx->deint) {
    in_ctx->dec_sub->pts = in_ctx->init_pkt->pts * 2;
  } else {
    in_ctx->dec_sub->pts = in_ctx->init_pkt->pts;
  }

  proc_ctx->last_sub_pts = in_ctx->dec_sub->pts;

  if ((ret = sub_to_frame_convert(proc_ctx->burn_in_ctx->stf_ctx, in_ctx)))
  {
    fprintf(stderr, "Failed to convert subtitle to frame.\n");
    return ret;
  }

  if ((ret = burn_in_subtitles(proc_ctx, stream_ctx,
    proc_ctx->burn_in_ctx->s_buffersrc_ctx,
    proc_ctx->burn_in_ctx->stf_ctx->subtitle_frame)) < 0)
  {
    fprintf(stderr, "Failed to filter and encode frame.\n");
    return ret;
  }

  return 0;
}

int decode_av_packet(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, InputContext *in_ctx)
{
  int ret = 0;

  if ((ret =
    avcodec_send_packet(stream_ctx->dec_ctx, in_ctx->init_pkt)) < 0)
  {
    fprintf(stderr, "Failed to send packet from input stream: %d to decoder.\n\
      Error: %s.\n", stream_ctx->in_stream_idx, av_err2str(ret));
    return ret;
  }

  while ((ret = avcodec_receive_frame(stream_ctx->dec_ctx,
    in_ctx->dec_frame)) >= 0)
  {
    in_ctx->dec_frame->pict_type = AV_PICTURE_TYPE_NONE;

    if (stream_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      if (proc_ctx->deint) {
        if ((ret = deinterlace_video_frame(proc_ctx, stream_ctx, in_ctx)) < 0) {
          fprintf(stderr, "Failed to deinterlace video frame from input stream: %d.\n",
            stream_ctx->in_stream_idx);
          return ret;
        }
      }
      else if (proc_ctx->burn_in_ctx && proc_ctx->first_sub) {
        if (in_ctx->dec_frame->pts > proc_ctx->last_sub_pts + 5000) {
          push_dummy_subtitle(proc_ctx, stream_ctx, in_ctx->dec_frame->pts);
          proc_ctx->last_sub_pts = in_ctx->dec_frame->pts;
        }

        if ((ret = burn_in_subtitles(proc_ctx, stream_ctx,
          proc_ctx->burn_in_ctx->v_buffersrc_ctx,
          in_ctx->dec_frame)) < 0)
        {
          fprintf(stderr, "Failed to burn in subtitles.\n");
          return ret;
        }
      }
      else if (proc_ctx->rend_ctx) {
        if ((ret = make_rendtion(proc_ctx, stream_ctx,
          in_ctx->dec_frame)) < 0)
        {
          fprintf(stderr, "Failed to make video renditions.\n");
          return ret;
        }
      }
      else {
        if ((ret = encode_video_frame(proc_ctx, stream_ctx,
          in_ctx->dec_frame, 0)) < 0)
        {
          fprintf(stderr, "Failed to encode video frame from input stream '%d'.\n",
            stream_ctx->in_stream_idx);
          return ret;
        }
      }
    }
    else if (stream_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if (stream_ctx->transcode_rend0) {
        in_ctx->dec_frame_cpy = av_frame_clone(in_ctx->dec_frame);

        if ((ret = convert_audio_frame(proc_ctx, stream_ctx, 0,
          in_ctx->dec_frame_cpy)) < 0)
        {
          fprintf(stderr, "Failed to encode audio frame from input stream '%d'.\n",
            stream_ctx->in_stream_idx);
          return ret;
        }

        av_frame_unref(in_ctx->dec_frame_cpy);
      }

      if (stream_ctx->renditions) {
        if ((ret = convert_audio_frame(proc_ctx, stream_ctx, 1,
          in_ctx->dec_frame)) < 0)
        {
          fprintf(stderr, "Failed to encode audio frame from input stream '%d'.\n",
            stream_ctx->in_stream_idx);
          return ret;
        }
      }
    }

    av_frame_unref(in_ctx->dec_frame);
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
    fprintf(stderr, "Failed to receive frame from input stream '%d' from decoder.\
      \nLibav Error: %s.\n", in_ctx->init_pkt->stream_index, av_err2str(ret));
    return ret;
  }

  return 0;
}

int decode_packet(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, InputContext *in_ctx)
{
  int ret = 0;

  if (
    stream_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
    stream_ctx->codec_type == AVMEDIA_TYPE_AUDIO
  ) {
    if ((ret = decode_av_packet(proc_ctx, stream_ctx, in_ctx)) < 0) {
      fprintf(stderr, "Failed to decode audio or video packet \
        for input stream '%d'.\n", stream_ctx->in_stream_idx);
      return ret;
    }
  } else if (
    stream_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE &&
    in_ctx->init_pkt
  ) {
    if ((ret = decode_sub_packet(proc_ctx, stream_ctx, in_ctx)) < 0) {
      fprintf(stderr, "Failed to decode subtitle packet \
        for input stream: %d.\n", stream_ctx->in_stream_idx);
      return ret;
    }
  }

  return 0;
}

int transcode(ProcessingContext *proc_ctx, InputContext *in_ctx,
  const char *batch_id, char *process_job_id)
{
  StreamContext *stream_ctx;
  int in_stream_idx, ctx_idx, frame_count, check_again = 0, out_stream_idx, ret = 0;
  enum AVMediaType codec_type;

  while ((ret = av_read_frame(in_ctx->fmt_ctx, in_ctx->init_pkt)) >= 0)
  {
    in_stream_idx = in_ctx->init_pkt->stream_index;
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    codec_type = in_ctx->fmt_ctx->streams[in_stream_idx]->codecpar->codec_type;

    if (codec_type == AVMEDIA_TYPE_VIDEO) {
      frame_count += 1;

      if (frame_count % 100 == 0) {
        if (check_abort_status(batch_id) == ABORTED) {
          return ABORTED;
        }
      }

      if (frame_count % 2000 == 0 || check_again) {
        if (in_ctx->init_pkt->pts > 0) {
          check_again = 0;
          if (calculate_pct_complete(in_ctx, process_job_id) < 0) {
            fprintf(stderr, "Failed to calculate percent complete.\n");
          }
        } else {
          check_again = 1;
        }
      }
    }

    if (ctx_idx == INACTIVE_STREAM) {
      if(in_ctx->init_pkt) { av_packet_unref(in_ctx->init_pkt); }
      continue;
    }

    stream_ctx = proc_ctx->stream_ctx_arr[ctx_idx];
    out_stream_idx = stream_ctx->rend0_out_stream_idx;

    if (stream_ctx->passthrough) {
      in_ctx->init_pkt->stream_index = out_stream_idx;

      if ((ret =
        av_interleaved_write_frame(proc_ctx->out_fmt_ctx, in_ctx->init_pkt)) < 0)
      {
        fprintf(stderr, "Failed to write packet from input stream '%d' to "
          "output stream '%d'.\nLibav Error: %s.\n",
          in_stream_idx, out_stream_idx, av_err2str(ret));
        return ret;
      }

      if(in_ctx->init_pkt) { av_packet_unref(in_ctx->init_pkt); }
      continue;
    }

    if (
      codec_type == AVMEDIA_TYPE_AUDIO &&
      !stream_ctx->transcode_rend0
    ) {
      in_ctx->init_pkt_cpy = av_packet_clone(in_ctx->init_pkt);
      in_ctx->init_pkt_cpy->stream_index = out_stream_idx;

      if ((ret =
        av_interleaved_write_frame(proc_ctx->out_fmt_ctx, in_ctx->init_pkt_cpy)) < 0)
      {
        fprintf(stderr, "Failed to write packet to file.\nError: %s.\n",
          av_err2str(ret));
        return ret;
      }

      if (in_ctx->init_pkt_cpy) { av_packet_unref(in_ctx->init_pkt_cpy); }
    }

    if ((ret = decode_packet(proc_ctx, stream_ctx,
      in_ctx)) < 0)
    {
      fprintf(stderr, "Failed to decode packet.\n");
      return ret;
    }

    if(in_ctx->init_pkt) { av_packet_unref(in_ctx->init_pkt); }
  }

  if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
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
  StreamContext *stream_ctx;
  InputContext *in_ctx = NULL;
  sqlite3 *db;

  av_log_set_level(AV_LOG_ERROR);

  if (update_process_job_status(process_job_id, PROCESSING) < 0) {
    fprintf(stderr, "Failed to update process job status.\n");
  }

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: %s\nSqlite Error: %s\n",
      DATABASE_URL, sqlite3_errmsg(db));
    ret = -ret;
    goto update_status;
  }

  if (!(proc_ctx = processing_context_alloc(process_job_id, db))) {
    fprintf(stderr, "Failed to allocate processing context.\n");
    goto update_status;
  }

  if ((ret = get_processing_info(proc_ctx, process_job_id, db)) < 0) {
    fprintf(stderr, "Failed to get prcessing info.\n");
    goto update_status;
  }

  if (!(in_ctx = open_input(proc_ctx, process_job_id, db))) {
    fprintf(stderr, "Failed to open input.\n");
    ret = -1;
    goto update_status;
  }

  if ((ret = open_output(proc_ctx, in_ctx, process_job_id, db)) < 0) {
    fprintf(stderr, "Failed to open output.\n");
    goto update_status;
  }

  if ((ret = processing_context_init(proc_ctx, in_ctx,
    process_job_id)) < 0)
  {
    fprintf(stderr, "Failed to initialize processing context.\n");
    goto update_status;
  }

  sqlite3_close(db);
  db = NULL;

  if ((ret = transcode(proc_ctx, in_ctx, batch_id, process_job_id)) < 0) {
    fprintf(stderr, "Failed to transcode.\n");
  }

  if (in_ctx->init_pkt) { av_packet_unref(in_ctx->init_pkt); }
  av_packet_free(&in_ctx->init_pkt);
  in_ctx->init_pkt = NULL;

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    if (ctx_idx == INACTIVE_STREAM) { continue; }

    stream_ctx = proc_ctx->stream_ctx_arr[ctx_idx];
    if (stream_ctx->passthrough) { continue; }
    out_stream_idx = stream_ctx->rend0_out_stream_idx;

    if (
      stream_ctx->renditions &&
      stream_ctx->codec_type == AVMEDIA_TYPE_AUDIO
    ) {
      out_stream_idx = stream_ctx->rend1_out_stream_idx;
    }

    if (decode_packet(proc_ctx, stream_ctx, in_ctx) < 0) {
      fprintf(stderr, "Failed to decode packet while flushing \
        decoder for input stream '%d'.\n", in_stream_idx);
    }
  }

  av_frame_unref(in_ctx->dec_frame);
  av_frame_free(&in_ctx->dec_frame);
  in_ctx->dec_frame = NULL;

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    out_stream_idx = stream_ctx->rend0_out_stream_idx;
    if (ctx_idx == INACTIVE_STREAM) { continue; }

    stream_ctx = proc_ctx->stream_ctx_arr[ctx_idx];

    if (stream_ctx->passthrough) { continue; }
    if (stream_ctx->renditions) { out_stream_idx += 1; }
    if (stream_ctx->codec_type != AVMEDIA_TYPE_AUDIO) { continue; }

    if (convert_audio_frame(proc_ctx, stream_ctx, 0,
      in_ctx->dec_frame) < 0)
    {
      fprintf(stderr, "Failed to encode audio frame while flushing \
        frame size converter for input stream '%d'.\n", in_stream_idx);
    }
  }

  if (proc_ctx->deint_ctx) {
    if (deinterlace_video_frame(proc_ctx, stream_ctx, in_ctx) < 0) {
      fprintf(stderr, "Failed to flush deinterlace filter.\n");
    }
  }

  if (proc_ctx->burn_in_ctx) {
    if (burn_in_subtitles(proc_ctx, stream_ctx,
      proc_ctx->burn_in_ctx->v_buffersrc_ctx, NULL) < 0) {
        fprintf(stderr, "Failed to flush burn in filter.\n");
    }
  }

  for (
    in_stream_idx = 0;
    in_stream_idx < (int) in_ctx->fmt_ctx->nb_streams;
    in_stream_idx++
  ) {
    ctx_idx = proc_ctx->ctx_map[in_stream_idx];
    if (ctx_idx == INACTIVE_STREAM) { continue; }

    stream_ctx = proc_ctx->stream_ctx_arr[ctx_idx];
    if (stream_ctx->passthrough) { continue; }
    out_stream_idx = stream_ctx->rend0_out_stream_idx;

    if (stream_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (encode_video_frame(proc_ctx, stream_ctx,
        NULL, 0) < 0)
      {
        fprintf(stderr, "Failed to encode video frame from input stream: %d.\n",
          in_stream_idx);
      }
    } else if (stream_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (stream_ctx->renditions) { out_stream_idx += 1; }

      if (encode_audio_frame(proc_ctx, stream_ctx, 0,
        NULL) < 0)
      {
        fprintf(stderr, "Failed to write audio frame.\n");
      }
    }
  }

  if (av_write_trailer(proc_ctx->out_fmt_ctx) < 0) {
    fprintf(stderr, "Failed to write trailer to file.\n\
      Libav Error: %s.\n", av_err2str(ret));
  }

  if (ret != ABORTED) {
    printf("pct_complete: 100%%\n");

    if (update_pct_complete(100, process_job_id) < 0) {
      fprintf(stderr, "Failed to update pct_complete to 100%%\n");
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
        fprintf(stderr, "Failed to process video for process job \"%s\".\n",
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
