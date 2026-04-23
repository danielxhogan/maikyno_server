#include "make_clips.h"

int make_clip(AVFormatContext *in_fmt_ctx, int v_stream_idx,
  char *out_filename, int start_sec, int duration_sec)
{
  int ret;
  int64_t start_ts, duration_ts, end_ts, first_dts;
  enum FIRST_DTS_SET first_dts_set = NOT_SET;
  AVFormatContext *out_fmt_ctx = NULL;
  AVPacket *pkt = NULL;
  AVStream *in_stream = NULL, *out_stream = NULL;

  if ((ret =
    avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, out_filename)))
  {
    fprintf(stderr, "Failed to allocate output format context.\n");
    goto end;
  }

  if (!(out_stream = avformat_new_stream(out_fmt_ctx, NULL))) {
    fprintf(stderr, "Failed to allocate video output stream.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if ((ret = avcodec_parameters_copy(out_stream->codecpar,
    in_fmt_ctx->streams[v_stream_idx]->codecpar)) < 0)
  {
    fprintf(stderr, "Failed to copy video codec parameters\n");
    goto end;
  }
  out_stream->codecpar->codec_tag = 0;

  start_ts =
    start_sec *
    in_fmt_ctx->streams[v_stream_idx]->time_base.den /
    in_fmt_ctx->streams[v_stream_idx]->time_base.num;

  if ((ret = av_seek_frame(in_fmt_ctx, v_stream_idx,
    start_ts, AVSEEK_FLAG_BACKWARD)) < 0)
  {
    fprintf(stderr, "Failed to seek to start frame.\n");
    goto end;
  }

  if (!(pkt = av_packet_alloc())) {
    fprintf(stderr, "Failed to allocate AVPacket.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    if ((ret =
      avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE)) < 0)
    {
      fprintf(stderr, "Failed to open output file.\n");
      goto end;
    }
  }

  if ((ret = avformat_write_header(out_fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to write header for output file.\n");
    goto end;
  }

  in_stream = in_fmt_ctx->streams[v_stream_idx];

  while ((ret = av_read_frame(in_fmt_ctx, pkt)) >= 0)
  {
    if (
      pkt->stream_index != v_stream_idx ||
      pkt->dts < 0 ||
      pkt->pts < 0
    ) {
      av_packet_unref(pkt);
      continue;
    }

    if (first_dts_set == NOT_SET)
    {
      if (!(pkt->flags & AV_PKT_FLAG_KEY)) {
        av_packet_unref(pkt);
        continue;
      }

      first_dts = pkt->dts;
      first_dts_set = SET;
      duration_ts = av_rescale_q(duration_sec * AV_TIME_BASE, AV_TIME_BASE_Q,
        in_fmt_ctx->streams[v_stream_idx]->time_base);
      end_ts = first_dts + duration_ts;
    }

    if (end_ts && pkt->dts > end_ts) {
      av_packet_unref(pkt);
      break;
    }

    pkt->pts = av_rescale_q(pkt->pts - first_dts,
      in_stream->time_base, out_stream->time_base);

    pkt->dts = av_rescale_q(pkt->dts - first_dts,
      in_stream->time_base, out_stream->time_base);

    pkt->duration = av_rescale_q(pkt->duration,
      in_stream->time_base,out_stream->time_base);

    pkt->pos = -1;
    if ((ret = av_interleaved_write_frame(out_fmt_ctx, pkt)) < 0) {
      fprintf(stderr, "Failed to write packet to file.\n");
      goto end;
    }

    av_packet_unref(pkt);
  }

  if ((ret = av_write_trailer(out_fmt_ctx)) < 0) {
    fprintf(stderr, "Failed to write trailer to file.\n");
    goto end;
  }

end:
  if (out_fmt_ctx && !(out_fmt_ctx->flags & AVFMT_NOFILE))
    avio_closep(&out_fmt_ctx->pb);
  avformat_free_context(out_fmt_ctx);
  av_packet_free(&pkt);

  return ret;
}

int make_clips(const char *video_file_path, const char *clips_dir_path,
  const char *process_job_id)
{
  #define MIN_DURATION_PERCENT 10
  #define MAX_DURATION_PERCENT 90
  #define DURATION_PERCENT_STEP 5
  #define SAMPLE_DURATION 3

  char clip_filename[LEN_CLIP_FILENAME + 1], *clip_path;
  const char *end;
  int v_stream_idx, len_clips_dir_path, clip_number,
    duration_percent, sample_start, ret;
  float duration;
  AVFormatContext *in_fmt_ctx = NULL;

  sqlite3 *db;
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

  sqlite3_close(db);
  db = NULL;

  if ((ret =
    avformat_open_input(&in_fmt_ctx, video_file_path, NULL, NULL)) < 0)
  {
    fprintf(stderr,
      "Failed to open input video file: '%s'.\n", video_file_path);
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

  duration = in_fmt_ctx->duration / AV_TIME_BASE;

  for (end = clips_dir_path; *end; end++);
  len_clips_dir_path = end - clips_dir_path;

  clip_number = 1;

  for (
    duration_percent = MIN_DURATION_PERCENT;
    duration_percent <= MAX_DURATION_PERCENT;
    duration_percent += DURATION_PERCENT_STEP
  ) {
    snprintf(clip_filename, LEN_CLIP_FILENAME + 1,
      "%0" LEN_CLIP_FILE_STEM_STRING "d.mkv", clip_number);
    clip_number++;

    if (!(clip_path =
      calloc(len_clips_dir_path + LEN_CLIP_FILENAME + 2, sizeof(char))))
    {
      fprintf(stderr, "Failed to allocate clip_path.\n");
      ret = -ENOMEM;
      goto end;
    }

    strncat(clip_path, clips_dir_path, len_clips_dir_path);
    strcat(clip_path, "/");
    strncat(clip_path, clip_filename, LEN_CLIP_FILENAME);

    sample_start = duration * duration_percent / 100;

    if ((ret = make_clip(in_fmt_ctx, v_stream_idx,
      clip_path, sample_start, SAMPLE_DURATION)) < 0)
    {
      fprintf(stderr, "Failed to make clip %s.\n", clip_filename);
      goto end;
    }
  
    free(clip_path);
  }

end:
  sqlite3_close(db);
  avformat_close_input(&in_fmt_ctx);

  if (ret < 0 && ret != AVERROR_EOF) {
    fprintf(stderr, "\nLibav Error: %s\n", av_err2str(ret));
    return -1;
  }

  return 0;
}
