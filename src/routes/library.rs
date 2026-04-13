use crate::db::{
  config::{
    db_connect::DBPool,
    models::{NewLibrary, NewLibraryDir, UpdateVideo}
  },
  library::{
    MediaType,
    create_library,
    create_library_dirs,
    select_library,
    select_libraries
  },
  media::{
    select_seasons_by_id,
    select_movies,
    select_media_dir,
    select_video,
    select_videos,
    update_video
  },
  show::{select_shows_by_id}
};

use crate::AppState;

use crate::utils::{
  mk_error::{MKError, MKErrorType, blocking_error},
  mk_fs::{mk_create_dir_all, mk_remove_dir_all, mk_read_dir}
};

use actix_web::{Responder, post, web};
use serde::Deserialize;
use serde_json;

use std::{
  ffi::OsStr,
  fs::{DirEntry, Metadata, ReadDir},
  os::unix::fs::{DirEntryExt, MetadataExt},
  path::{Path, PathBuf}
};

#[derive(Deserialize)]
struct NewLibraryInfo {
  name: String,
  paths: Vec<String>,
  media_type: String
}

#[derive(Deserialize)]
struct NewLibraryDirInfo {
  library_id: String,
  paths: Vec<String>,
}

#[derive(Deserialize)]
struct GetMoviesInfo {
  library_id: String,
}

#[derive(Deserialize)]
struct GetShowsParams {
  library_id: String,
}

#[derive(Deserialize)]
struct GetSeasonsParams {
  show_id: String,
}

#[derive(Deserialize)]
struct GetVideosInfo {
  media_dir_id: String,
}

#[derive(Deserialize)]
struct UpdateVideoPlaybackStateParams {
  video_id: String,
  ts: i32,
  v_stream: i32,
  a_stream: i32,
  s_stream: i32,
  s_pos: i32
}

fn create_new_library_dirs(new_paths: Vec<String>, root_library_dir: &String,
  library_name: &String) -> Result< Vec<NewLibraryDir>, MKError>
{
  let err_msg: String;
  let mut new_library_path: PathBuf;
  let mut new_library_parent: &Path;
  let mut new_library_parent_dirs: ReadDir;
  let mut dir_entry: DirEntry;
  let mut new_library_dir_name: &OsStr;
  let mut ino_opt: Option<String>;
  let mut ino: String;
  let mut device_id_metadata: Metadata;
  let mut device_id_opt: Option<String>;
  let mut device_id: String;
  let mut symlink_path: String;
  let mut static_path: String;
  let mut new_library_dir: NewLibraryDir;
  let mut new_library_dirs: Vec<NewLibraryDir> = vec![];

  for new_path in new_paths.clone() {
    ino_opt = None;
    device_id_opt = None;
    new_library_path = PathBuf::from(&new_path);

    if !new_library_path.exists() {
      err_msg = format!("{:?}: {:?}",
        MKErrorType::LibraryPathNotFoundError.to_string(), &new_library_path);
      eprintln!("{err_msg:?}");
      return Err(MKError::new(
        MKErrorType::LibraryPathNotFoundError, err_msg).into());
    }

    new_library_parent = match new_library_path.parent()
    {
      Some(new_library_parent) => { new_library_parent },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::ParentDirError.to_string(), &new_library_path);
        println!("{err_msg}");
        return Err(MKError::new(MKErrorType::ParentDirError, err_msg).into());
      }
    };

    new_library_parent_dirs =
      match mk_read_dir(&new_library_parent.to_path_buf())
      {
        Ok(new_library_parent_dirs) => { new_library_parent_dirs },
        Err(err) => {
          err_msg = format!("{:?}", err.message);
          eprintln!("{err_msg}");
          return Err(err.into()); }
      };

    new_library_dir_name = match &new_library_path.file_name()
    {
      Some(new_library_dir_name) => { new_library_dir_name },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::FileNameError.to_string(), &new_library_path);
        eprintln!("{err_msg}");
        return Err(MKError::new(MKErrorType::FileNameError, err_msg).into());
      }
    };

    for dir_entry_result in new_library_parent_dirs
    {
      dir_entry = match dir_entry_result {
        Ok(dir_entry) => { dir_entry },
        Err(err) => {
          eprintln!("Error: {:?}", err);
          continue;
        }
      };

      if dir_entry.file_name() == new_library_dir_name.to_os_string() {
        ino_opt = Some(dir_entry.ino().to_string());

        device_id_metadata = match dir_entry.metadata()
        {
          Ok(device_id_metadata) => { device_id_metadata },
          Err(err) => {
            err_msg = format!("{:?}: {:?}\nError: {:?}",
              MKErrorType::DirEntryMetadataError.to_string(),
              dir_entry.path(), err);
            eprintln!("{err_msg}");
            return Err(MKError::new(
              MKErrorType::DirEntryMetadataError, err_msg).into());
          }
        };

        device_id_opt = Some(device_id_metadata.dev().to_string());
        break;
      }
    }

    ino = match ino_opt
    {
      Some(ino) => { ino },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::LibraryPathNotFoundError.to_string(), &new_library_path);
        eprintln!("{err_msg:?}");
        return Err(MKError::new(
          MKErrorType::LibraryPathNotFoundError, err_msg).into());
      }
    };

    device_id = match device_id_opt
    {
      Some(device_id) => { device_id },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::LibraryPathNotFoundError.to_string(), &new_library_path);
        eprintln!("{err_msg:?}");
        return Err(MKError::new(
          MKErrorType::LibraryPathNotFoundError, err_msg).into());
      }
    };

    symlink_path = format!("{}/{}",
      root_library_dir, new_library_dir_name.to_string_lossy());

    static_path = format!("{}/{}",
      library_name, &new_library_dir_name.to_string_lossy());

    new_library_dir = NewLibraryDir {
      name: new_library_dir_name.to_string_lossy().to_string(),
      real_path: new_path.clone(),
      symlink_path: symlink_path.clone(),
      static_path: static_path.clone(),
      ino: ino,
      device_id: device_id,
    };

    new_library_dirs.push(new_library_dir);
  }

  return Ok(new_library_dirs);
}

#[post("/new_library")]
pub async fn new_library(new_library_info: web::Json<NewLibraryInfo>,
  pool: web::Data<DBPool>, app_state: web::Data<AppState>)
  -> actix_web::Result<String>
{
  let media_type: MediaType;
  let mut err_msg: String;

  if new_library_info.media_type == MediaType::Movie.to_string() {
    media_type = MediaType::Movie;
  }
  else if new_library_info.media_type == MediaType::Show.to_string() {
    media_type = MediaType::Show;
  }
  else {
    err_msg = format!("{:?}: {:?}",
      MKErrorType::InvalidMediaType.to_string(), &new_library_info.media_type);

    eprintln!("{err_msg}");
    return Err(MKError::new(MKErrorType::InvalidMediaType, err_msg).into());
  }

  let root_library_dir = format!("{}/{}",
    app_state.root_media_dir_string, new_library_info.name);

  if PathBuf::from(&root_library_dir).exists() {
    let err_msg = format!("Library: {:?} already exists.",
    &new_library_info.name);

    eprintln!("{err_msg}");
    return Err(MKError::new(MKErrorType::LibraryAlreadyExists, err_msg).into())
  }

  match mk_create_dir_all(&root_library_dir) {
    Ok(_) => {},
    Err(err) => { return Err(err.into()); }
  };

  let mut new_library_dirs: Vec<NewLibraryDir> = vec![];

  new_library_dirs = match create_new_library_dirs(new_library_info.paths.clone(),
    &root_library_dir, &new_library_info.name)
  {
    Ok(new_library_dirs) => {new_library_dirs},
    Err(err) => {
      err_msg = err.message;

      match mk_remove_dir_all(&root_library_dir) {
        Ok(_) => {},
        Err(err) => {
          err_msg = format!("{:?}\n{:?}", err_msg, err.message);
        }
      }

      eprintln!("{err_msg:?}");
      return Err(MKError::new(
        MKErrorType::LibraryPathNotFoundError, err_msg).into());
    }
  };

  for new_library_dir in new_library_dirs.clone()
  {
    #[cfg(target_family = "unix")]
    match std::os::unix::fs::symlink(&new_library_dir.real_path,
      &new_library_dir.symlink_path)
    {
      Ok(_) => {},
      Err(err) => {
        err_msg = format!("{:?}: {:?}\nPath: {:?}\nError: {:?}",
          MKErrorType::LibrarySymlinkError.to_string(),
          new_library_dir.symlink_path, &new_library_dir.real_path, err);

        match mk_remove_dir_all(&root_library_dir) {
          Ok(_) => {},
          Err(err) => {
            err_msg = format!("{:?}\n{:?}", err_msg, err.message);
          }
        }
        
        eprintln!("{err_msg:?}");
        return Err(MKError::new(
          MKErrorType::LibrarySymlinkError, err_msg).into());
      }
    }
  }

  let new_library = NewLibrary {
    name: new_library_info.name.clone(),
    media_type: media_type.to_string()
  };

  let pool_clone = pool.clone();
  let block_thread_result = web::block(|| {
    return create_library(pool_clone, new_library, new_library_dirs);
  }).await;

  let library = match block_thread_result
  {
    Ok(create_library_result) => {
      match create_library_result
      {
        Ok(library) => { library },
        Err(err) => {
          match mk_remove_dir_all(&root_library_dir) {
            Ok(_) => {},
            Err(rm_dir_err) => {
              err_msg = format!("{:?}\n{:?}", err.message, rm_dir_err.message);
              eprintln!("{err_msg}");
              return Err(MKError::new(err.err_type, err_msg).into());
            }
          }
          return Err(err.into());
        }
      }
    },
    Err(err) => {
      match mk_remove_dir_all(&root_library_dir) {
        Ok(_) => {},
        Err(rm_dir_err) => {
          err_msg = format!("{:?}\n{:?}", err, rm_dir_err.message);
          eprintln!("{err_msg}");
          return Err(blocking_error(err).into());
        }
      }
      return Err(blocking_error(err).into());
    }
  };

  match serde_json::to_string(&library) {
    Ok(library) => { Ok(library) },
    Err(err) => {
      err_msg = format!("{:?}: {:?}\nSuccessfully created library: {:?}",
        MKErrorType::SerializeError.to_string(), err, library.name);
      eprintln!("{err_msg}");
      return Err(MKError::new(MKErrorType::SerializeError, err_msg).into());
    }
  }
}

#[post("/add_library_dirs")]
pub async fn add_library_dirs(new_library_dir_info: web::Json<NewLibraryDirInfo>,
  pool: web::Data<DBPool>, app_state: web::Data<AppState>)
  -> actix_web::Result<String>
{
  let pool_clone = pool.clone();
  let library_id_clone = new_library_dir_info.library_id.clone();
  let block_thread_result = web::block(|| {
    return select_library(pool_clone, library_id_clone);
  }).await;

  let library = match block_thread_result
  {
    Ok(library_result) => {
      match library_result
      {
        Ok(library) => { library },
        Err(err) => {
          return Err(err.into());
        }
      }
    },
    Err(err) => {
      return Err(blocking_error(err).into());
    }
  };

  let root_library_dir = format!("{}/{}",
    app_state.root_media_dir_string, library.name);

  let new_library_dirs =
    match create_new_library_dirs(new_library_dir_info.paths.clone(),
      &root_library_dir, &library.name)
    {
      Ok(new_library_dirs) => { new_library_dirs },
      Err(err) => {
        return Err(err.into());
      }
    };

  let new_library_dirs_clone = new_library_dirs.clone();
  let block_thread_result = web::block(|| {
    return create_library_dirs(pool, new_library_dirs_clone, library.id);
  }).await;

  match block_thread_result {
    Ok(create_library_dirs_result) => {
      match create_library_dirs_result {
        Ok(_) => {},
        Err(err) => {
          return Err(err.into());
        }
      }
    },
    Err(err) => {
      return Err(blocking_error(err).into());
    }
  }

  for new_library_dir in new_library_dirs
  {
    #[cfg(target_family = "unix")]
    match std::os::unix::fs::symlink(&new_library_dir.real_path,
      &new_library_dir.symlink_path)
    {
      Ok(_) => {},
      Err(err) => {
        let err_msg = format!("{:?}: {:?}\nPath: {:?}\nError: {:?}",
          MKErrorType::LibrarySymlinkError.to_string(),
          new_library_dir.symlink_path, &new_library_dir.real_path, err);
        eprintln!("{err_msg:?}");
        return Err(MKError::new(
          MKErrorType::LibrarySymlinkError, err_msg).into());
      }
    }
  }

  return Ok("hi".to_string());
}

#[post("/get_libraries")]
pub async fn get_libraries(pool: web::Data<DBPool>)
  -> impl Responder
{
  let block_thread_result = web::block(|| {
    return select_libraries(pool);
  }).await;

  let libraries = match block_thread_result
  {
    Ok(libraries_result) => {
      match libraries_result
      {
        Ok(libraries) => { libraries },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  return Ok(web::Json(libraries));
}

#[post("/get_shows")]
pub async fn get_shows(get_shows_params: web::Json<GetShowsParams>,
  pool: web::Data<DBPool>) -> impl Responder
{
  let library_id_clone = get_shows_params.library_id.clone();
  let block_thread_result = web::block(|| {
    return select_shows_by_id(library_id_clone, pool);
  }).await;

  let shows = match block_thread_result
  {
    Ok(shows_result) => {
      match shows_result
      {
        Ok(shows) => { shows },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  return Ok(web::Json(shows));
}

#[post("/get_seasons")]
pub async fn get_seasons(get_seasons_params: web::Json<GetSeasonsParams>,
  pool: web::Data<DBPool>) -> impl Responder
{
  let show_id_clone = get_seasons_params.show_id.clone();
  let block_thread_result = web::block(|| {
    return select_seasons_by_id(show_id_clone, pool);
  }).await;

  let seasons = match block_thread_result
  {
    Ok(seasons_result) => {
      match seasons_result
      {
        Ok(seasons) => { seasons },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  return Ok(web::Json(seasons));
}

#[post("/get_movies")]
pub async fn get_movies(get_movies_info: web::Json<GetMoviesInfo>,
  pool: web::Data<DBPool>) -> impl Responder
{
  let pool_clone = pool.clone();
  let library_id_clone = get_movies_info.library_id.clone();
  let block_thread_result = web::block(|| {
    return select_library(pool_clone, library_id_clone);
  }).await;

  let library = match block_thread_result
  {
    Ok(library_result) => {
      match library_result
      {
        Ok(library) => { library },
        Err(err) => { return  Err(err); }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  let block_thread_result = web::block(|| {
    return select_movies(pool, library)
  }).await;

  let media_dirs = match block_thread_result
  {
    Ok(media_dirs_result) => {
      match media_dirs_result
      {
        Ok(media_dirs) => { media_dirs },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  return Ok(web::Json(media_dirs));
}

#[post("/get_videos")]
pub async fn get_videos(get_videos_info: web::Json<GetVideosInfo>,
  pool: web::Data<DBPool>) -> impl Responder
{
  let pool_clone = pool.clone();
  let media_dir_id_clone = get_videos_info.media_dir_id.clone();
  let block_thread_result = web::block(|| {
    return select_media_dir(pool_clone, media_dir_id_clone);
  }).await;

  let media_dir = match block_thread_result
  {
    Ok(media_dir_result) => {
      match media_dir_result
      {
        Ok(media_dir) => { media_dir },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  let block_thread_result = web::block(|| {
    return select_videos(pool, media_dir);
  }).await;

  let videos = match block_thread_result
  {
    Ok(videos_result) => {
      match videos_result
      {
        Ok(videos) => { videos },
        Err(err) => { return Err(err) }
      }
    },
    Err(err) => {
      return Err(blocking_error(err));
    }
  };

  return Ok(web::Json(videos));
}

#[post("/update_video_playback_state")]
pub async fn update_video_playback_state(
  update_video_playback_state_params: web::Json<UpdateVideoPlaybackStateParams>,
  pool: web::Data<DBPool>) -> actix_web::Result<String>
{
  let pool_clone = pool.clone();
  let video_id_clone = update_video_playback_state_params.video_id.clone();
  let block_thread_result = web::block(|| {
    return select_video(pool_clone, video_id_clone);
  }).await;

  let video = match block_thread_result
  {
    Ok(video_result) => {
      match video_result
      {
        Ok(video) => { video },
        Err(err) => {
          return Err(err.into());
        }
      }
    },
    Err(err) => {
      return Err(blocking_error(err).into());
    }
  };

  let update_video_info = UpdateVideo {
    id: video.id,
    name: video.name,
    real_path: Some(video.real_path),
    static_path: Some(video.static_path),
    extra: video.extra,
    processed: video.processed,
    ts: update_video_playback_state_params.ts,
    v_stream: update_video_playback_state_params.v_stream,
    a_stream: update_video_playback_state_params.a_stream,
    s_stream: update_video_playback_state_params.s_stream,
    s_pos: update_video_playback_state_params.s_pos
  };

  let block_thread_result = web::block(|| {
    return update_video(pool, update_video_info);
  }).await;

  let updated_video = match block_thread_result
  {
    Ok(updated_video_result) => {
      match updated_video_result
      {
        Ok(update_video) => { update_video },
        Err(err) => {
          return Err(err.into());
        }
      }
    },
    Err(err) => {
      return Err(blocking_error(err).into());
    }
  };

  return Ok("hi".to_string());
}
