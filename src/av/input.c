#include "input.h"

int get_input_file(char **input_file, char *process_job_id, sqlite3 *db)
{
  int ret, len_input_file;
  const unsigned char *input_file_tmp, *end;

  char *select_video_path_query =
    "SELECT videos.real_path FROM process_jobs \
    JOIN videos ON process_jobs.video_id = videos.id \
    WHERE process_jobs.id = ?;";
  sqlite3_stmt *select_video_path_stmt;

  if ((ret = sqlite3_prepare_v2(db, select_video_path_query, -1,
    &select_video_path_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select video path statement.\nError%s\n",
      sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_video_path_stmt, 1, process_job_id, -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_video_path_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to get path for video for process_job: %s\nError: %s\n",
      process_job_id, sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if (!(input_file_tmp = sqlite3_column_text(select_video_path_stmt, 0))) {
    fprintf(stderr, "Failed to get column text for select video path query \
      for process_job: %s\nError: %s\n", process_job_id, sqlite3_errmsg(db));
    ret = -1;
    goto end;
  }

  for (end = input_file_tmp; *end; end++);
  len_input_file = end - input_file_tmp;

  if (!(*input_file = calloc(len_input_file + 1, sizeof(char))))
  {
    fprintf(stderr, "Failed to allocate input_file\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  memcpy(*input_file, input_file_tmp, len_input_file + 1);

end:
  sqlite3_finalize(select_video_path_stmt);
  return ret;
}

static enum AVPixelFormat hw_pix_fmt;

int init_hw_dec_ctx(ProcessingContext *proc_ctx, const AVCodec *dec)
{
  const AVCodecHWConfig *config;

  for (int i = 0;; i++)
  {
    if(!(config = avcodec_get_hw_config(dec, i))) {
      fprintf(stderr, "Decoder %s does not support device type %s.\n",
        dec->name, av_hwdevice_get_type_name(proc_ctx->hw_type));
      break;
    }

    if (
      config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
      config->device_type == proc_ctx->hw_type
    ) {
      hw_pix_fmt = config->pix_fmt;
      proc_ctx->hw_dec = 1;

      if (hw_pix_fmt != proc_ctx->hw_pix_fmt) {
        printf("hw_pix_fmt mismatch:\nhw_pix_fmt: %d\n"
          "proc_ctx->hw_pix_fmt: %d\n", hw_pix_fmt, proc_ctx->hw_pix_fmt);
        proc_ctx->hw_pix_fmt = hw_pix_fmt;
      }
      break;
    }
  }

  return 0;
}

enum AVPixelFormat get_hw_fmt(AVCodecContext *dec_ctx,
  const enum AVPixelFormat *pix_fmts)
{
  const enum AVPixelFormat *pix_fmt;

  for (pix_fmt = pix_fmts; *pix_fmt != -1; pix_fmt++) {
    if (*pix_fmt == hw_pix_fmt) { return *pix_fmt; }
  }

  fprintf(stderr, "Failed to get hardware pixel format.\n");
  return AV_PIX_FMT_NONE;
}

static int open_decoder(ProcessingContext *proc_ctx,
  StreamContext *stream_ctx, int in_stream_idx)
{
  int ret = 0;
  const AVCodec *dec;

  AVStream *stream = proc_ctx->in_fmt_ctx->streams[in_stream_idx];

  if (!(dec = avcodec_find_decoder(stream->codecpar->codec_id))) {
    fprintf(stderr, "Failed to find decoder.\n");
    ret = AVERROR(EINVAL);
    return ret;
  }

  if (
    proc_ctx->hw_ctx &&
    (unsigned int) stream_ctx->in_stream_idx == proc_ctx->v_stream_idx
  ) {
    if ((ret = init_hw_dec_ctx(proc_ctx, dec)) < 0) {
      fprintf(stderr, "Failed to initialize hardware decoder.\n");
    }
  }

  if (!(stream_ctx->dec_ctx = avcodec_alloc_context3(dec))) {
    fprintf(stderr, "Failed to allocate decoder context "
      "for stream_idx: %d.\n", stream->index);
    ret = AVERROR(ENOMEM);
    return ret;
  }

  if ((ret =
    avcodec_parameters_to_context(stream_ctx->dec_ctx,
      stream->codecpar)) < 0)
  {
    fprintf(stderr, "Failed to copy codec parameters to decoder context "
      "for stream_idx: %d.\n", stream->index);
    return ret;
  }

  if (proc_ctx->hw_dec) {
    if (!(stream_ctx->dec_ctx->hw_device_ctx = av_buffer_ref(proc_ctx->hw_ctx))) {
      fprintf(stderr, "Failed to create reference between decoder "
        "context and hardware device context.\n");
      return AVERROR(ENOMEM);
    }

    stream_ctx->dec_ctx->get_format = get_hw_fmt;
  }


  if ((ret = avcodec_open2(stream_ctx->dec_ctx, dec, NULL)) < 0) {
    fprintf(stderr, "Failed to open decoder for stream_idx: %d.\n",
      stream->index);
    return ret;
  }

  return 0;
}

int open_input(ProcessingContext *proc_ctx,
  char *process_job_id, sqlite3 *db)
{
  int in_stream_idx, ret = 0;
  char *input_file = NULL;
  StreamContext *stream_ctx;
  AVDictionary *opts = NULL;

  if ((ret = get_input_file(&input_file, process_job_id, db)) < 0)
  {
    fprintf(stderr,
      "Failed to get input file for process_job: %s\n", process_job_id);
    goto end;
  }

  printf("Opening input file \"%s\".\n", input_file);

  if ((ret = av_dict_set(&opts, "probesize", "50000000", 0)) < 0) {
    fprintf(stderr, "Failed to set probesize option.\n");
    goto end;
  }

  if ((ret = av_dict_set(&opts, "analyzeduration", "10000000", 0)) < 0) {
    fprintf(stderr, "Failed to set analyzeduration option.\n");
    goto end;
  }

  if ((ret =
    avformat_open_input(&proc_ctx->in_fmt_ctx, input_file, NULL, &opts)) < 0)
  {
    fprintf(stderr, "Failed to open input file \"%s\".\n", input_file);
    goto end;
  }

  if ((ret = avformat_find_stream_info(proc_ctx->in_fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info for: %s\n",
      input_file);
    goto end;
  }

  for (unsigned int i = 0; i < proc_ctx->nb_selected_streams; i++) {
    stream_ctx = proc_ctx->stream_ctx_arr[i];

    in_stream_idx = stream_ctx->in_stream_idx;
    stream_ctx->in_stream = proc_ctx->in_fmt_ctx->streams[in_stream_idx];
    stream_ctx->codec_type = stream_ctx->in_stream->codecpar->codec_type;

    if (stream_ctx->passthrough) { continue; }

    if ((ret = open_decoder(proc_ctx, stream_ctx, in_stream_idx)) < 0) {
      fprintf(stderr, "Failed to open decoder.\n");
      fprintf(stderr, "stream '%d'.\nprocess job: \"%s\".\n",
        in_stream_idx, process_job_id);
      goto end;
    }
  }

end:
  free(input_file);

  if (ret < 0) {
    fprintf(stderr, "Libav Error: %s.\n", av_err2str(ret));
    return ret;
  }

  return 0;
}
