use crate::db::{
  config::{
    db_connect::{get_db_conn, DBPool},
    models::{
      Library,
      Show,
      MediaDir,
      NewMediaDir,
      NewVideo,
      UpdateMediaDir,
      UpdateVideo,
      Video
    },
    schema::{media_dirs, videos}
  },
  collection::delete_collection_movies_for_movie
};

use crate::utils::mk_error::{MKError, MKErrorType};

use actix_web::web;
use diesel::prelude::*;
use uuid::Uuid;

pub fn create_media_dir(pool: web::Data<DBPool>, new_media_dir: NewMediaDir)
  -> Result<MediaDir, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let media_dir = MediaDir {
    id: Uuid::new_v4().to_string(),
    ino: new_media_dir.ino,
    device_id: new_media_dir.device_id,
    name: new_media_dir.name,
    real_path: new_media_dir.real_path.clone(),
    symlink_path: new_media_dir.symlink_path,
    static_path: new_media_dir.static_path,
    thumbnail_url: new_media_dir.thumbnail_url,
    library_id: new_media_dir.library_id.clone(),
    library_dir_id: new_media_dir.library_dir_id,
    show_id: new_media_dir.show_id.clone()
  };

  let media_dir_result = diesel::insert_into(media_dirs::table)
    .values(&media_dir).get_result(&mut db)
    .map_err(|err|
    {
      let err_msg: String;
      match new_media_dir.show_id
      {
        Some(some_show_id) => {
          err_msg = format!("Failed to create new season: {:?} for show: {:?}
            \nError: {:?}", new_media_dir.real_path, some_show_id, err);
        },
        None => {
          err_msg = format!("Failed to create new movie: {:?} for library: {:?}
            \nError: {:?}", new_media_dir.real_path, new_media_dir.library_id, err);
        }
      }

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return media_dir_result;
}

pub fn select_media_dir(pool: web::Data<DBPool>, media_dir_id: String)
  -> Result<MediaDir, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let media_dir_result = media_dirs::table
    .filter(media_dirs::id.eq(&media_dir_id))
    .get_result::<MediaDir>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get media dir with id '{}'.", media_dir_id);
      let err_msg = format!("{err_msg} Error: {:?}", err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return media_dir_result;
}

pub fn select_movies(pool: web::Data<DBPool>, library: Library)
  -> Result<Vec<MediaDir>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let media_result = media_dirs::table
    .filter(media_dirs::library_id.eq(&library.id))
    .order_by(media_dirs::name)
    .get_results::<MediaDir>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get movies for library:
        {:?}, {:?}\nError: {:?}", library.name, library.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return media_result;
}

pub fn select_seasons(pool: web::Data<DBPool>, show: Show)
  -> Result<Vec<MediaDir>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let media_result = media_dirs::table
    .filter(media_dirs::show_id.eq(&show.id))
    .get_results::<MediaDir>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get seasons for show:
        {:?}, {:?}\nError: {:?}", show.name, show.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return media_result;
}

pub fn select_seasons_by_id(show_id: String, pool: web::Data<DBPool>)
  -> Result<Vec<MediaDir>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let media_result = media_dirs::table
    .filter(media_dirs::show_id.eq(&show_id))
    .get_results::<MediaDir>(&mut db)
    .map_err(|err| {
      let err_msg = format!(
        "Failed to get seasons for show_id: {:?}\nError: {:?}",
        show_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return media_result;
}

pub fn update_media_dir(pool: web::Data<DBPool>,
  update_media_dir: UpdateMediaDir) -> Result<MediaDir, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let updated_media_dir_result = diesel::update(media_dirs::table
    .filter(media_dirs::id.eq(&update_media_dir.id)))
    .set((
      media_dirs::name.eq(&update_media_dir.name),
      media_dirs::real_path.eq(&update_media_dir.real_path),
      media_dirs::symlink_path.eq(&update_media_dir.symlink_path),
      media_dirs::static_path.eq(&update_media_dir.static_path)
    ))
    .get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to update media dir:
        {:?}, {:?}\nError: {:?}",
        update_media_dir.real_path, update_media_dir.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return updated_media_dir_result;
}

pub fn delete_media_dir(pool: web::Data<DBPool>, media_dir: MediaDir)
  -> Result<MediaDir, MKError>
{
  let pool_clone = pool.clone();
  let media_dir_clone = media_dir.clone();

  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let delete_media_dir_result = db.transaction(|db|
  {
    match delete_videos(pool_clone.clone(), media_dir_clone.clone()) {
      Ok(_) => {},
      Err(err) => { return Err(err); }
    }

    match delete_collection_movies_for_movie(pool_clone, media_dir_clone) {
      Ok(_) => {},
      Err(err) => { return Err(err); }
    }

    return diesel::delete(media_dirs::table)
      .filter(media_dirs::id.eq(&media_dir.id))
      .get_result::<MediaDir>(db)
      .map_err(|err| {
        let err_ctx_msg = format!("Failed to delete collection movies for movie:
        {:?}, {:?}\nError: {:?}", media_dir.real_path, media_dir.id, err);

        eprintln!("{err_ctx_msg:?}");
        return MKError::new(MKErrorType::DBError, err_ctx_msg);
      });
  });

  return delete_media_dir_result;
}

pub fn delete_seasons_for_show(pool: web::Data<DBPool>, show: Show)
  -> Result<String, MKError>
{
  let pool_clone = pool.clone();
  let show_clone = show.clone();

  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let delete_seasons_result = db.transaction(|_|
  {
    let seasons = match select_seasons(pool_clone.clone(), show_clone) {
      Ok(seasons) => { seasons },
      Err(err) => { return Err(err); }
    };

    for season in seasons {
      match delete_media_dir(pool_clone.clone(), season) {
        Ok(_) => {},
        Err(err) => { return Err(err); }
      }
    }

    return Ok(format!("Successfully deleted all season for show: {:?}, {:?}",
      show.real_path, show.id));
  });

  return delete_seasons_result;
}

pub fn create_video(pool: web::Data<DBPool>, new_video: NewVideo)
  -> Result<Video, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let video = Video {
    id: Uuid::new_v4().to_string(),
    name: new_video.name.clone(),
    title: None,
    suggested_title: None,
    og_path: new_video.og_path,
    static_path: new_video.static_path,
    bitrate: None,
    extra: new_video.extra,
    processed: new_video.processed,
    thumbnail_url: new_video.thumbnail_url,
    ts: 0,
    pct_watched: 0,
    finished: false,
    v_stream: 1,
    a_stream: 1,
    s_stream: 1,
    s_pos: 100,
    media_dir_id: new_video.media_dir_id.clone()
  };

  let video_result = diesel::insert_into(videos::table)
    .values(&video).get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to create video: {:?} for media dir: {:?}
        \nError: {:?}", new_video.name, new_video.media_dir_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return video_result;
}

pub fn select_video(pool: web::Data<DBPool>, video_id: String)
  -> Result<Video, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let video_result = videos::table
    .filter(videos::id.eq(&video_id))
    .get_result::<Video>(&mut db)
    .map_err(|err| {
      let mut err_msg = format!("Failed to get video with id: '{}'. ", video_id);
      err_msg = format!("{err_msg}Error: {}.", err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return video_result;
}

pub fn select_videos(pool: web::Data<DBPool>, media_dir: MediaDir)
  -> Result<Vec<Video>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let videos_result = videos::table
    .filter(videos::media_dir_id.eq(&media_dir.id))
    .order_by(videos::name)
    .get_results::<Video>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get videos for media dir:
        {:?}, {:?}\nError: {:?}", media_dir.name, media_dir.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return videos_result;
}

pub fn select_videos_by_id(pool: web::Data<DBPool>, media_dir_id: String)
  -> Result<Vec<Video>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let videos_result = videos::table
    .filter(videos::media_dir_id.eq(&media_dir_id))
    .order_by(videos::name)
    .get_results::<Video>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get videos for media dir:
        {:?}\nError: {:?}", media_dir_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return videos_result;
}

pub fn update_video(pool: web::Data<DBPool>, update_video: UpdateVideo)
  -> Result<Video, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let prev_video = match videos::table
    .filter(videos::id.eq(&update_video.id))
    .get_result::<Video>(&mut db)
  {
    Ok(prev_video) => { prev_video },
    Err(err) => {
      let err_msg = format!("Failed to get video:
        {:?}, {:?}\nError: {:?}", update_video.name, update_video.id, err);

      eprintln!("{err_msg:?}");
      return Err(MKError::new(MKErrorType::DBError, err_msg));
    }
  };

  let name = match update_video.name {
    Some(name) => { name },
    None => { prev_video.name }
  };

  let title = match update_video.title {
    Some(title) => { title },
    None => { prev_video.title }
  };

  let suggested_title = match update_video.suggested_title {
    Some(suggested_title) => { suggested_title },
    None => { prev_video.suggested_title }
  };

  let og_path = match update_video.og_path {
    Some(og_path) => { og_path },
    None => { prev_video.og_path }
  };

  let static_path = match update_video.static_path {
    Some(static_path) => { static_path },
    None => { prev_video.static_path }
  };

  let bitrate = match update_video.bitrate {
    Some(bitrate) => { bitrate },
    None => { prev_video.bitrate }
  };

  let extra = match update_video.extra {
    Some(extra) => { extra },
    None => { prev_video.extra }
  };

  let processed = match update_video.processed {
    Some(processed) => { processed },
    None => { prev_video.processed }
  };

  let thumbnail_url = match update_video.thumbnail_url {
    Some(thumbnail_url) => { thumbnail_url },
    None => { prev_video.thumbnail_url }
  };

  let ts = match update_video.ts {
    Some(ts) => { ts },
    None => { prev_video.ts }
  };

  let pct_watched = match update_video.pct_watched {
    Some(pct_watched) => { pct_watched },
    None => { prev_video.pct_watched }
  };

  let finished = match update_video.finished {
    Some(finished) => { finished },
    None => { prev_video.finished }
  };

  let v_stream = match update_video.v_stream {
    Some(v_stream) => { v_stream },
    None => { prev_video.v_stream }
  };

  let a_stream = match update_video.a_stream {
    Some(a_stream) => { a_stream },
    None => { prev_video.a_stream }
  };

  let s_stream = match update_video.s_stream {
    Some(s_stream) => { s_stream },
    None => { prev_video.s_stream }
  };

  let s_pos = match update_video.s_pos {
    Some(s_pos) => { s_pos },
    None => { prev_video.s_pos }
  };

  let updated_video_result = diesel::update(videos::table
    .filter(videos::id.eq(&update_video.id)))
    .set((
      videos::name.eq(&name),
      videos::title.eq(title),
      videos::suggested_title.eq(suggested_title),
      videos::og_path.eq(og_path),
      videos::static_path.eq(static_path),
      videos::bitrate.eq(bitrate),
      videos::extra.eq(extra),
      videos::processed.eq(processed),
      videos::thumbnail_url.eq(thumbnail_url),
      videos::ts.eq(ts),
      videos::pct_watched.eq(pct_watched),
      videos::finished.eq(finished),
      videos::v_stream.eq(v_stream),
      videos::a_stream.eq(a_stream),
      videos::s_stream.eq(s_stream),
      videos::s_pos.eq(s_pos),
    ))
    .get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to update video: {:?}, {:?}\nError: {:?}",
        name, update_video.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return updated_video_result;
}

pub fn delete_video(pool: web::Data<DBPool>, video: Video)
  -> Result<Option<Video>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_video_result = diesel::delete(videos::table)
    .filter(videos::id.eq(&video.id))
    .get_result::<Video>(&mut db).optional()
    .map_err(|err| {
      let err_msg = format!("Failed to delete video: {:?}, {:?}\nError: {:?}",
        video.name, video.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_video_result;
}

pub fn delete_videos(pool: web::Data<DBPool>, media_dir: MediaDir)
  -> Result<Vec<Video>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_videos_result = diesel::delete(videos::table)
    .filter(videos::id.eq(&media_dir.id))
    .get_results::<Video>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to delete videos for media dir:
        {:?}, {:?}\nError: {:?}", media_dir.real_path, media_dir.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_videos_result;
}
