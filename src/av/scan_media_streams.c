#include "scan_media_streams.h"

int generate_suggested_title(char **suggested_title,
  char *video_name, char *media_dir_name,
  char *show_id, int extra, sqlite3 *db)
{
  int len_video_name, len_show_name, len_media_dir_name, len_extra_str,
    len_episode_str, len_suggested_title, ret = 0;

  char *video_name_root = NULL, *dot_pos, *show_name,
    *extra_str = " Extra ", *episode_str = " Episode ",
    *end;

  char *select_show_name_query = "SELECT name FROM shows WHERE id = ?;";
  sqlite3_stmt *select_show_name_stmt = NULL;

  for (end = video_name; *end; end++);
  len_video_name = end - video_name;

  if (!(video_name_root = calloc(len_video_name + 1, sizeof(char))))
  {
    fprintf(stderr, "Failed to allocate video_name_root.\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  memcpy(video_name_root, video_name, len_video_name + 1);
  dot_pos = strchr(video_name_root, '.');
  *dot_pos = '\0';

  for (end = media_dir_name; *end; end++);
  len_media_dir_name = end - media_dir_name;

  for (end = extra_str; *end; end++);
  len_extra_str = end - extra_str;

  if (show_id)
  {
    if ((ret = sqlite3_prepare_v2(db, select_show_name_query, -1,
      &select_show_name_stmt, 0)) != SQLITE_OK)
    {
      fprintf(stderr, "Failed to prepare select show name statement.\n");
      fprintf(stderr, "show_id: \"%s\".\n", show_id);
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -ret;
      goto end;
    }

    sqlite3_bind_text(select_show_name_stmt, 1, show_id, -1, SQLITE_STATIC);

    if ((ret = sqlite3_step(select_show_name_stmt)) != SQLITE_ROW) {
      fprintf(stderr, "Failed to select show name for show: \"%s\".\n", show_id);
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -ret;
      goto end;
    }

    if (!(show_name = (char *) sqlite3_column_text(select_show_name_stmt, 0)))
    {
      fprintf(stderr, "Failed to get show name show: \"%s\".\n", show_id);
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -1;
      goto end;
    }

    for (end = show_name; *end; end++);
    len_show_name = end - show_name;

    if (extra) {
      len_suggested_title =
        len_show_name +
        1 +
        len_media_dir_name +
        len_extra_str +
        len_video_name;

      if (!(*suggested_title = calloc(len_suggested_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate suggested_title for season extra.\n");
        fprintf(stderr, "show_id: \"%s\".\n", show_id);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strcat(*suggested_title, show_name);
      strcat(*suggested_title, " ");
      strcat(*suggested_title, media_dir_name);
      strcat(*suggested_title, extra_str);
    }
    else {
      for (end = episode_str; *end; end++);
      len_episode_str = end - episode_str;

      len_suggested_title =
        len_show_name +
        1 +
        len_media_dir_name +
        len_episode_str +
        len_video_name;

      if (!(*suggested_title = calloc(len_suggested_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate suggested_title for episode.\n");
        fprintf(stderr, "show_id: \"%s\".\n", show_id);
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strcat(*suggested_title, show_name);
      strcat(*suggested_title, " ");
      strcat(*suggested_title, media_dir_name);
      strcat(*suggested_title, episode_str);
    }
  }
  else {
    if (extra) {
      len_suggested_title =
        len_media_dir_name +
        len_extra_str +
        len_video_name;

      if (!(*suggested_title = calloc(len_suggested_title + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate suggested_title for movie extra.\n");
        ret = AVERROR(ENOMEM);
        goto end;
      }

      strcat(*suggested_title, media_dir_name);
      strcat(*suggested_title, " Extra ");
    }
    else {
      len_suggested_title = len_video_name;

      if (!(*suggested_title = calloc(len_video_name + 1, sizeof(char))))
      {
        fprintf(stderr, "Failed to allocate suggested_title for movie extra.\n");
        ret = AVERROR(ENOMEM);
        goto end;
      }
    }
  }

  strcat(*suggested_title, video_name_root);

end:
  sqlite3_finalize(select_show_name_stmt);
  free(video_name_root);
  return ret;
}

int scan_video(char *video_id, char *video_name,
  char *video_path, char *media_dir_name,
  char *show_id, int extra, sqlite3 *db)
{
  AVFormatContext *fmt_ctx = NULL;
  AVStream *stream;
  AVDictionaryEntry *title_dict_entry;

  uuid_t uuid;
  char uuid_str[LEN_UUID_STRING], *title, *suggested_title = NULL;
  const char *codec_name, *profile_name;
  int stream_id, height, width, interlaced, stream_type, ret = 0;

  char *update_video_query =
    "UPDATE videos \
    SET title = ?, suggested_title = ?, bitrate = ? \
    WHERE id = ?;";
  sqlite3_stmt *update_video_stmt = NULL;

  char *insert_video_stream_query =
    "INSERT INTO streams \
    (id, stream_idx, title, stream_type, codec, \
    height, width, interlaced, video_id) \
    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt *insert_video_stream_stmt = NULL;

  char *delete_video_streams_query =
    "DELETE FROM streams WHERE video_id = ?;";
  sqlite3_stmt *delete_video_streams_stmt = NULL;

  if ((ret = sqlite3_prepare_v2(db, delete_video_streams_query, -1,
    &delete_video_streams_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare delete movie version stream statement.");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(delete_video_streams_stmt, 1,
    (const char *) video_id, -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(delete_video_streams_stmt)) != SQLITE_DONE) {
    fprintf(stderr, "Failed to delete video streams.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if ((ret = avformat_open_input(&fmt_ctx, video_path, NULL, NULL)) < 0) {
    fprintf(stderr, "Failed to open input video.\n");
    fprintf(stderr, "Libav Error: %s.\n", av_err2str(ret));
    goto end;
  }

  if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream info.");
    fprintf(stderr, "Libav Error: %s.\n", av_err2str(ret));
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, update_video_query, -1,
    &update_video_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare update video statement.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  title_dict_entry = av_dict_get(fmt_ctx->metadata, "title", NULL, 0);
  title = title_dict_entry ? title_dict_entry->value : NULL;

  if ((ret = generate_suggested_title(&suggested_title, video_name,
    media_dir_name, show_id, extra, db)) < 0)
  {
    fprintf(stderr, "Failed to get suggested title.\n");
    goto end;
  }

  if (title) {
    sqlite3_bind_text(update_video_stmt, 1, title,
      -1, SQLITE_STATIC);
  }
  else {
    sqlite3_bind_null(update_video_stmt, 1);
  }

  sqlite3_bind_text(update_video_stmt, 2, suggested_title,
    -1, SQLITE_STATIC);

  sqlite3_bind_int(update_video_stmt, 3, fmt_ctx->bit_rate);

  sqlite3_bind_text(update_video_stmt, 4, video_id,
    -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(update_video_stmt)) != SQLITE_DONE) {
    fprintf(stderr, "Failed to update video.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
  {
    stream_id = (int) i;
    stream = fmt_ctx->streams[i];
    stream_type = stream->codecpar->codec_type;

    if (
      stream_type != AVMEDIA_TYPE_VIDEO &&
      stream_type != AVMEDIA_TYPE_AUDIO &&
      stream_type != AVMEDIA_TYPE_SUBTITLE
    ) { continue; }

    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    title_dict_entry = av_dict_get(stream->metadata, "title", NULL, 0);
    title = title_dict_entry ? title_dict_entry->value : NULL;
    codec_name = avcodec_get_name(stream->codecpar->codec_id);
    profile_name = avcodec_profile_name(stream->codecpar->codec_id,
      stream->codecpar->profile);

    if (profile_name && !strcmp(profile_name, "DTS-HD MA")) {
      codec_name = profile_name;
    }

    if (stream_type == AVMEDIA_TYPE_VIDEO) {
      height = stream->codecpar->height;
      width = stream->codecpar->width;
      
      if (stream->codecpar->field_order == AV_FIELD_PROGRESSIVE ||
        stream->codecpar->field_order == AV_FIELD_UNKNOWN)
      {
        interlaced = 0;
      }
      else { interlaced = 1; }
    }

  if ((ret = sqlite3_prepare_v2(db, insert_video_stream_query, -1,
    &insert_video_stream_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare insert video stream statement.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -ret;
    goto end;
  }

    sqlite3_bind_text(insert_video_stream_stmt, 1, uuid_str, -1, SQLITE_STATIC);
    sqlite3_bind_int(insert_video_stream_stmt, 2, stream_id);

    if (title) {
      sqlite3_bind_text(insert_video_stream_stmt, 3, title, -1, SQLITE_STATIC);
    } else {
      sqlite3_bind_null(insert_video_stream_stmt, 3);
    }

    sqlite3_bind_int(insert_video_stream_stmt, 4, stream_type);
    sqlite3_bind_text(insert_video_stream_stmt, 5, codec_name, -1, SQLITE_STATIC);

    if (stream_type == AVMEDIA_TYPE_VIDEO) {
      sqlite3_bind_int(insert_video_stream_stmt, 6, height);
      sqlite3_bind_int(insert_video_stream_stmt, 7, width);
      sqlite3_bind_int(insert_video_stream_stmt, 8, interlaced);
    }
    else {
      sqlite3_bind_null(insert_video_stream_stmt, 6);
      sqlite3_bind_null(insert_video_stream_stmt, 7);
      sqlite3_bind_null(insert_video_stream_stmt, 8);
    }

    sqlite3_bind_text(insert_video_stream_stmt, 9,
      (char *) video_id, -1, SQLITE_STATIC);

    if ((ret = sqlite3_step(insert_video_stream_stmt)) != SQLITE_DONE) {
      fprintf(stderr, "Failed to insert video stream.\n");
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
        ret = -ret;
      goto end;
    }
  }

end:
  avformat_close_input(&fmt_ctx);
  sqlite3_finalize(update_video_stmt);
  sqlite3_finalize(insert_video_stream_stmt);
  sqlite3_finalize(delete_video_streams_stmt);
  free(suggested_title);
  return ret;
}

int scan_media_streams(const char *media_dir_id)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *select_media_dir_stmt = NULL, *select_videos_stmt = NULL;

  char *select_media_dir_query =
    "SELECT name, real_path, show_id FROM media_dirs WHERE id = ?;";
  char *select_videos_query =
    "SELECT id, name, real_path, extra FROM videos WHERE media_dir_id = ?;";

  char *media_dir_name, *media_dir_path, *show_id,
    *video_id, *video_name, *video_path;
  int ret, extra;

  if ((ret = sqlite3_open(DATABASE_URL, &db)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to open database: \"%s\".\n", DATABASE_URL);
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if ((ret = sqlite3_prepare_v2(db, select_media_dir_query, -1,
    &select_media_dir_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select media dir statement.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_media_dir_stmt, 1, media_dir_id, -1, SQLITE_STATIC);

  if ((ret = sqlite3_step(select_media_dir_stmt)) != SQLITE_ROW) {
    fprintf(stderr, "Failed to select media dir.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  if (!(media_dir_name = (char *) sqlite3_column_text(select_media_dir_stmt, 0)))
  {
    fprintf(stderr, "Failed to get media dir name.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -1;
    goto end;
  }

  if (!(media_dir_path = (char *) sqlite3_column_text(select_media_dir_stmt, 1)))
  {
    fprintf(stderr, "Failed to get media_dir path.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -1;
    goto end;
  }

  show_id = (char *) sqlite3_column_text(select_media_dir_stmt, 2);

  if ((ret = sqlite3_prepare_v2(db, select_videos_query, -1,
    &select_videos_stmt, 0)) != SQLITE_OK)
  {
    fprintf(stderr, "Failed to prepare select videos statement.\n");
    fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
    ret = -ret;
    goto end;
  }

  sqlite3_bind_text(select_videos_stmt, 1, media_dir_id, -1, SQLITE_STATIC);

  while ((ret = sqlite3_step(select_videos_stmt)) == SQLITE_ROW)
  {
    if (!(video_id =
      (char *) sqlite3_column_text(select_videos_stmt, 0)))
    {
      fprintf(stderr, "Failed to get video id.\n");
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -1;
      continue;
    }

    if (!(video_name =
      (char *) sqlite3_column_text(select_videos_stmt, 1)))
    {
      fprintf(stderr, "Failed to get video_name.\nvideo_id: \"%s\".\n",
        video_id);
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -1;
      continue;
    }


    if (!(video_path =
      (char *) sqlite3_column_text(select_videos_stmt, 2)))
    {
      fprintf(stderr, "Failed to get video_name.\nvideo_id: \"%s\".\n",
        video_id);
      fprintf(stderr, "video_name: \"%s\".\n", video_name);
      fprintf(stderr, "Sqlite Error: %s.\n", sqlite3_errmsg(db));
      ret = -1;
      continue;
    }

    extra = sqlite3_column_int(select_videos_stmt, 3);

    if ((ret = scan_video(video_id, video_name, video_path,
      media_dir_name, show_id, extra, db)) < 0)
    {
      fprintf(stderr, "Failed to scan video.\nvideo_id: \"%s\".\n", video_id);
      fprintf(stderr, "video_name: \"%s\".\nvideo_path: \"%s\".\n",
        video_name, video_path);
      continue;
    }
  }

end:
  sqlite3_finalize(select_media_dir_stmt);
  sqlite3_finalize(select_videos_stmt);
  sqlite3_close(db);

  if (ret < 0) {
    fprintf(stderr, "Failed to scan media streams for media dir: \"%s\".\n",
      media_dir_id);
    return ret;
  }

  return 0;
}
