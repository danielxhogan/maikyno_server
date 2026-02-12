use crate::db::config::schema::{
  libraries,
  library_dirs,
  collections,
  collection_movies,
  collection_shows,
  shows,
  media_dirs,
  videos,
  process_jobs,
  process_job_video_streams,
  process_job_audio_streams,
  process_job_subtitle_streams,
  batches
};

use diesel::prelude::*;
use serde::{Deserialize, Serialize};

#[derive(Debug, Queryable, Selectable, Insertable, Serialize, Clone)]
#[diesel(table_name = libraries)]
pub struct Library {
  pub id: String,
  pub name: String,
  pub media_type: String
}

#[derive(Debug, Insertable, Clone)]
#[diesel(table_name = libraries)]
pub struct NewLibrary {
  pub name: String,
  pub media_type: String
}

#[derive(Debug, Queryable, Insertable, Clone)]
#[diesel(table_name = library_dirs)]
pub struct LibraryDir {
  pub id: String,
  pub ino: String,
  pub device_id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
  pub library_id: String
}

pub struct NewLibraryDir {
  pub ino: String,
  pub device_id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
}

#[derive(Debug, Queryable, Insertable, Clone)]
#[diesel(table_name = collections)]
pub struct Collection {
  pub id: String,
  pub name: String,
  pub ino: Option<String>,
  pub device_id: Option<String>,
  pub library_id: Option<String>
}

#[derive(Debug)]
pub struct NewCollection {
  pub name: String,
  pub ino: Option<String>,
  pub device_id: Option<String>,
  pub library_id: Option<String>
}

pub struct UpdateCollection {
  pub id: String,
  pub name: String
}

#[derive(Debug, Queryable, Insertable, Clone)]
#[diesel(table_name = collection_shows)]
pub struct CollectionShow {
  pub id: String,
  pub show_id: String,
  pub collection_id: String
}

pub struct NewCollectionShow {
  pub show_id: String,
  pub show_path: String,
  pub collection_id: String,
  pub collection_name: String
}

#[derive(Debug, Queryable, Clone)]
pub struct CollectionShowInfo {
  pub id: String,
  pub collection_id: String,
  pub collection_name: String,
  pub show_id: String,
  pub ino: String,
  pub device_id: String,
  // pub name: String,
  pub show_path: String,
  // pub symlink_path: String,
  // pub static_path: String,
  // pub library_id: String,
  // pub library_dir_id: String,
}

pub struct DeleteCollectionShow {
  pub id: String,
  pub show_id: String,
  pub show_path: String,
  pub collection_id: String,
  pub collection_name: String
}

#[derive(Debug, Queryable, Insertable, Clone)]
#[diesel(table_name = collection_movies)]
pub struct CollectionMovie {
  pub id: String,
  pub movie_id: String,
  pub collection_id: String
}

pub struct NewCollectionMovie {
  pub movie_id: String,
  pub movie_path: String,
  pub collection_id: String,
  pub collection_name: String
}

#[derive(Debug, Queryable, Clone)]
pub struct CollectionMovieInfo {
  pub id: String,
  pub collection_id: String,
  pub collection_name: String,
  pub movie_id: String,
  pub ino: String,
  pub device_id: String,
  // pub name: String,
  pub movie_path: String,
  // pub symlink_path: String,
  // pub static_path: String,
  // pub thumbnail_url: Option<String>,
  // pub library_id: String,
  // pub library_dir_id: String,
  // pub show_id: Option<String>
}

pub struct DeleteCollectionMovie {
  pub id: String,
  pub movie_id: String,
  pub movie_path: String,
  pub collection_id: String,
  pub collection_name: String
}

#[derive(Debug, Insertable, Queryable, Clone)]
#[diesel(table_name = shows)]
pub struct Show {
  pub id: String,
  pub ino: String,
  pub device_id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
  pub library_id: String,
  pub library_dir_id: String,
}

pub struct NewShow {
  pub ino: String,
  pub device_id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
  pub library_id: String,
  pub library_dir_id: String,
}

pub struct UpdateShow {
  pub id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
}

#[derive(Debug, Insertable, Queryable, Clone, Deserialize)]
#[diesel(table_name = media_dirs)]
pub struct MediaDir {
  pub id: String,
  pub ino: String,
  pub device_id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
  pub thumbnail_url: Option<String>,
  pub library_id: String,
  pub library_dir_id: String,
  pub show_id: Option<String>
}

impl MediaDir {
  pub fn new() -> MediaDir {
    return MediaDir {
      id:"".to_string(),
      ino: "".to_string(),
      device_id: "".to_string(),
      name: "".to_string(),
      real_path: "".to_string(),
      symlink_path: "".to_string(),
      static_path: "".to_string(),
      thumbnail_url: None,
      library_id: "".to_string(),
      library_dir_id: "".to_string(),
      show_id: None
    };
  }
}

#[derive(Debug)]
pub struct NewMediaDir {
  pub ino: String,
  pub device_id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String,
  pub thumbnail_url: Option<String>,
  pub library_id: String,
  pub library_dir_id: String,
  pub show_id: Option<String>
}

pub struct UpdateMediaDir {
  pub id: String,
  pub name: String,
  pub real_path: String,
  pub symlink_path: String,
  pub static_path: String
}

#[derive(Debug, Insertable, Queryable, Clone, Deserialize)]
#[diesel(table_name = videos)]
pub struct Video {
  pub id: String,
  pub name: String,
  pub title: Option<String>,
  pub suggested_title: Option<String>,
  pub real_path: String,
  pub static_path: String,
  pub bitrate: Option<i32>,
  pub extra: bool,
  pub processed: bool,
  pub thumbnail_url: Option<String>,
  pub media_dir_id: String
}

pub struct NewVideo {
  pub name: String,
  pub real_path: String,
  pub static_path: String,
  pub extra: bool,
  pub processed: bool,
  pub thumbnail_url: Option<String>,
  pub media_dir_id: String
}

pub struct UpdateVideo {
  pub id: String,
  pub name: String,
  pub real_path: Option<String>,
  pub static_path: Option<String>,
  pub extra: bool,
  pub processed: bool
}

// proc
// *****************************************************
#[derive(Debug, Insertable, Queryable, Clone)]
#[diesel(table_name = process_jobs)]
pub struct ProcessJob {
  pub id: String,
  pub title: Option<String>,
  pub job_status: String,
  pub stream_count: Option<i32>,
  pub pct_complete: i32,
  pub err_msg: Option<String>,
  pub video_id: String,
  pub batch_id: String,
}

#[derive(Debug, Insertable, Queryable, Clone)]
#[diesel(table_name = process_job_video_streams)]
pub struct  ProcessJobVideoStream {
  pub id: String,
  pub title: Option<String>,
  pub passthrough: bool,
  pub codec: i32,
  pub hwaccel: bool,
  pub create_renditions: bool,
  pub title2: Option<String>,
  pub tonemap: bool,
  pub deinterlace: bool,
  pub stream_id: String,
  pub process_job_id: String
}

#[derive(Debug, Insertable, Queryable, Clone)]
#[diesel(table_name = process_job_audio_streams)]
pub struct  ProcessJobAudioStream {
  pub id: String,
  pub title: Option<String>,
  pub passthrough: bool,
  pub gain_boost: i32,
  pub create_renditions: bool,
  pub title2: Option<String>,
  pub gain_boost2: i32,
  pub stream_id: String,
  pub process_job_id: String
}

#[derive(Debug, Insertable, Queryable, Clone)]
#[diesel(table_name = process_job_subtitle_streams)]
pub struct  ProcessJobSubtitleStream {
  pub id: String,
  pub title: Option<String>,
  pub burn_in: bool,
  pub stream_id: String,
  pub process_job_id: String
}

#[derive(Debug, Insertable, Queryable, Clone)]
#[diesel(table_name = batches)]
pub struct  Batch {
  pub id: String,
  pub batch_size: i32,
  pub aborted: bool
}
