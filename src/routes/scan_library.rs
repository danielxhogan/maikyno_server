use std::{
  collections::HashMap,
  fs::{DirEntry, ReadDir},
  os::unix::fs::{DirEntryExt, MetadataExt},
  path::PathBuf
};

use crate::db::{
  config::{
    db_connect::DBPool,
    models::{
      Library,
      LibraryDir,
      Collection,
      NewCollection,
      UpdateCollection,
      NewCollectionShow,
      CollectionShowInfo,
      DeleteCollectionShow,
      NewCollectionMovie,
      CollectionMovieInfo,
      DeleteCollectionMovie,
      Show,
      NewShow,
      UpdateShow,
      MediaDir,
      NewMediaDir,
      UpdateMediaDir,
      Video,
      NewVideo,
      UpdateVideo,
    }
  },
  library::{
    select_library,
    select_library_dirs,
    MediaType
  },
  collection::{
    create_collection,
    select_collections,
    update_collection,
    delete_collection,
    create_collection_show,
    select_collection_shows,
    delete_collection_show,
    create_collection_movie,
    select_collection_movies,
    delete_collection_movie
  },
  show::{
    create_show,
    select_shows,
    update_show,
    delete_show
  },
  media::{
    create_media_dir,
    select_movies,
    select_seasons,
    update_media_dir,
    delete_media_dir,
    select_videos,
    create_video,
    update_video,
    delete_video
  },
};

use crate::utils::{
  mk_error::{MKError, blocking_error},
  mk_fs::{
    mk_device_id_metadata, mk_pathbuf_file_name,
    mk_read_dir, mk_pathbuf_to_string
  }
};

use actix_web::{post, web};
use serde::Deserialize;

#[derive(Deserialize, Clone)]
struct ScanLibraryInfo {
  library_id: String
}

#[derive(Clone)]
struct PathAccumulator {
  symlink_path: String,
  static_path: String
}

struct LibraryIds {
  library_id: String,
  library_dir_id: String
}

struct ScanMediaRecursionState<'a> {
  media_dir_id: &'a String,
  videos_seen: &'a mut VideosSeen
}

struct ScanShowsCollectionInfo<'a> {
  collection_shows_seen: &'a mut CollectionShowsSeen,
  collection: &'a Collection
}
  
struct ScanMoviesCollectionInfo<'a> {
  collection_movies_seen: &'a mut CollectionMoviesSeen,
  collection: &'a Collection
}

#[derive(Eq, Hash, PartialEq)]
struct DirEntryId {
  ino: String,
  device_id: String
}

struct CollectionSeen {
  collection: Collection,
  seen: bool
}

type CollectionsSeen = HashMap<DirEntryId, CollectionSeen>;

struct CollectionShowSeen {
  collection_show: CollectionShowInfo,
  seen: bool
}

type CollectionShowsSeen = HashMap<DirEntryId, CollectionShowSeen>;

struct CollectionMovieSeen {
  collection_movie: CollectionMovieInfo,
  seen: bool
}

type CollectionMoviesSeen = HashMap<DirEntryId, CollectionMovieSeen>;

struct ShowSeen {
  show: Show,
  seen: bool
}

type ShowsSeen = HashMap<DirEntryId, ShowSeen>;

struct MediaDirSeen {
  media_dir: MediaDir,
  seen: bool
}

type MediaSeen = HashMap<DirEntryId, MediaDirSeen>;

struct VideoSeen {
  video: Video,
  seen: bool,
  processed: bool
}

type VideosSeen = HashMap<String, VideoSeen>;

async fn get_library_record(library_id: &String,
  pool: &web::Data<DBPool>) -> Result<Library, MKError>
{
  let pool_clone = pool.clone();
  let library_id_clone = library_id.clone();

  let block_thread_result = web::block(move || {
    return select_library(pool_clone, library_id_clone);
  }).await;

  let library_record = match block_thread_result
  {
    Ok(select_library_result) => {
      match select_library_result {
        Ok(library_record) => { library_record },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  return Ok(library_record);
}

async fn get_library_dir_records(library: &Library,
  pool: &web::Data<DBPool>) -> Result<Vec<LibraryDir>, MKError>
{
  let pool_clone = pool.clone();
  let library_clone = library.clone();

  let block_thread_result = web::block(move || {
    return select_library_dirs(pool_clone, library_clone);
  }).await;

  let library_dir_records = match block_thread_result
  {
    Ok(select_library_dirs_result) => {
      match select_library_dirs_result {
        Ok(library_dir_records) => { library_dir_records },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  return Ok(library_dir_records);
}


async fn get_initial_collections(library: &Library, pool: &web::Data<DBPool>)
  -> Result<CollectionsSeen, MKError>
{
  let library_clone = library.clone();
  let pool_clone = pool.clone();

  let block_thread_result = web::block(move || {
    return select_collections(pool_clone,
      library_clone);
  }).await;

  let library_collection_records = match block_thread_result
  {
    Ok(select_library_collections_result) => {
      match select_library_collections_result
      {
        Ok(lib_collection_records) => { lib_collection_records },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)) }
  };

  let collections_seen: CollectionsSeen =
    library_collection_records.clone().into_iter()
      .filter(|collection| {
        collection.ino != None
      })
      .filter(|collection| {
        collection.device_id != None
      })
      .map(|collection| {
        let direntry_id = DirEntryId {
          ino: collection.ino.clone().unwrap(),
          device_id: collection.device_id.clone().unwrap()
        };

        let collection_seen = CollectionSeen {
          collection: collection,
          seen: false
        };

        return (direntry_id, collection_seen);
      })
      .collect();

  return Ok(collections_seen);
}

async fn get_initial_collection_shows(collection: &Collection,
  pool: &web::Data<DBPool>)
  -> Result<CollectionShowsSeen, MKError>
{
  let pool_clone = pool.clone();
  let collection_clone = collection.clone();

  let block_thread_result = web::block(move || {
    return select_collection_shows(pool_clone, collection_clone);
  }).await;

  let collection_show_records = match block_thread_result
  {
    Ok(select_collection_shows_result) => {
      match select_collection_shows_result
      {
        Ok(collection_shows) => { collection_shows},
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let collection_shows_seen: CollectionShowsSeen =
    collection_show_records.clone().into_iter()
      .map(|collection_show|
      {
        let direntry_id = DirEntryId {
          ino: collection_show.ino.clone(),
          device_id: collection_show.device_id.clone()
        };

        let collection_show_seen = CollectionShowSeen {
          collection_show: collection_show,
          seen: false
        };

        return (direntry_id, collection_show_seen);
      })
      .collect();

  return Ok(collection_shows_seen);
}

async fn get_initial_collection_movies(collection: &Collection,
  pool: &web::Data<DBPool>)
  -> Result<CollectionMoviesSeen, MKError>
{
  let pool_clone = pool.clone();
  let collection_clone = collection.clone();

  let block_thread_result = web::block(move || {
    return select_collection_movies(pool_clone, collection_clone);
  }).await;

  let collection_movie_records = match block_thread_result
  {
    Ok(select_collection_movies_result) => {
      match select_collection_movies_result
      {
        Ok(collection_movies) => { collection_movies},
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let collection_movies_seen: CollectionMoviesSeen =
    collection_movie_records.clone().into_iter()
      .map(|collection_movie|
      {
        let direntry_id = DirEntryId {
          ino: collection_movie.ino.clone(),
          device_id: collection_movie.device_id.clone()
        };

        let collection_movie_seen = CollectionMovieSeen {
          collection_movie: collection_movie,
          seen: false
        };

        return (direntry_id, collection_movie_seen);
      })
      .collect();

  return Ok(collection_movies_seen);
}

async fn get_initial_shows(library: &Library, pool: &web::Data<DBPool>)
  -> Result<ShowsSeen, MKError>
{
  let pool_clone = pool.clone();
  let library_clone = library.clone();

  let block_thread_result = web::block(move || {
    return select_shows(pool_clone,
      library_clone);
  }).await;

  let show_records = match block_thread_result
  {
    Ok(select_shows_result) => {
      match select_shows_result {
        Ok(show_records) => { show_records },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let shows_seen: ShowsSeen =
    show_records.clone().into_iter()
      .map(|show|
      {
        let direntry_id = DirEntryId {
          ino: show.ino.clone(),
          device_id: show.device_id.clone()
        };

        let show_seen = ShowSeen {
          show: show,
          seen: false
        };

        return (direntry_id, show_seen);
      })
      .collect();

  return Ok(shows_seen);
}

async fn get_initial_seasons(show: &Show, pool: &web::Data<DBPool>)
  -> Result<MediaSeen, MKError>
{
  let pool_clone = pool.clone();
  let show_clone = show.clone();

  let block_thread_result = web::block(move || {
    return select_seasons(pool_clone, show_clone);
  }).await;

  let media_dir_records = match block_thread_result
  {
    Ok(select_library_media_result) => {
      match select_library_media_result {
        Ok(media_dir_records) => { media_dir_records },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let media_seen: MediaSeen =
    media_dir_records.clone().into_iter()
      .map(|media_dir|
      {
        let direntry_id = DirEntryId {
          ino: media_dir.ino.clone(),
          device_id: media_dir.device_id.clone()
        };

        let media_dir_seen = MediaDirSeen {
          media_dir: media_dir,
          seen: false
        };

        return (direntry_id, media_dir_seen);
      })
      .collect();

  return Ok(media_seen);
}

async fn get_initial_movies(library: &Library, pool: &web::Data<DBPool>)
  -> Result<MediaSeen, MKError>
{
  let pool_clone = pool.clone();
  let library_clone = library.clone();

  let block_thread_result = web::block(move || {
    return select_movies(pool_clone,
      library_clone);
  }).await;

  let media_dir_records = match block_thread_result
  {
    Ok(select_library_media_result) => {
      match select_library_media_result {
        Ok(media_dir_records) => { media_dir_records },
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let media_seen: MediaSeen =
    media_dir_records.clone().into_iter()
      .map(|media_dir|
      {
        let direntry_id = DirEntryId {
          ino: media_dir.ino.clone(),
          device_id: media_dir.device_id.clone()
        };

        let media_dir_seen = MediaDirSeen {
          media_dir: media_dir,
          seen: false
        };

        return (direntry_id, media_dir_seen);
      })
      .collect();

  return Ok(media_seen);
}

async fn get_initial_videos(media_dir: &MediaDir, pool: &web::Data<DBPool>)
  -> Result<VideosSeen, MKError>
{
  let pool_clone = pool.clone();
  let media_dir_clone = media_dir.clone();

  let block_thread_result = web::block(move || {
    return select_videos(pool_clone, media_dir_clone);
  }).await;

  let video_records = match block_thread_result
  {
    Ok(select_videos_result) => {
      match select_videos_result
      {
        Ok(video_records) => { video_records},
        Err(err) => { return Err(err); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let videos_seen: VideosSeen =
    video_records.clone().into_iter()
      .map(|video|
      {

        let video_seen = VideoSeen {
          video: video.clone(),
          seen: false,
          processed: false
        };

        return (video.name, video_seen);
      })
      .collect();

  return Ok(videos_seen);
}

async fn check_seen_media_dir(media_dir: &DirEntry,
  path_accumulator: &mut PathAccumulator,
  media_seen: &mut MediaSeen,
  library_ids: &LibraryIds, show_id: Option<&String>,
  pool: &web::Data<DBPool>
) -> Result<MediaDir, MKError>
{
  let device_id_metadata = match mk_device_id_metadata(&media_dir) {
    Ok(metadata) => { metadata },
    Err(err) => { return Err(err); }
  };

  let device_id = device_id_metadata.dev().to_string();
  let ino = media_dir.ino().to_string();

  let media_dir_file_name = match mk_pathbuf_file_name(&media_dir.path()) {
    Ok(collection_name) => { collection_name },
    Err(err) => { return Err(err); }
  };

  let media_dir_path = match mk_pathbuf_to_string(&media_dir.path()) {
    Ok(path) => { path },
    Err(err) => { return Err(err); }
  };

  let direntry_id = DirEntryId {
    ino: ino.clone(),
    device_id: device_id.clone()
  };

  let media_dir: MediaDir;

  match media_seen.get_mut(&direntry_id) {
    Some(media_dir_seen) => {
      media_dir_seen.seen = true;
      media_dir = media_dir_seen.media_dir.clone();

      let update_media_dir_info = UpdateMediaDir {
        id: media_dir.id.clone(),
        name: media_dir_file_name,
        real_path: media_dir_path,
        symlink_path: path_accumulator.symlink_path.clone(),
        static_path: path_accumulator.static_path.clone(),
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return update_media_dir(pool_clone, update_media_dir_info);
      }).await;

      match block_thread_result
      {
        Ok(update_media_dir_result) => {
          match update_media_dir_result
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

    },
    None => {
      let new_media_dir = NewMediaDir {
        ino: ino.clone(),
        device_id: device_id.clone(),
        name: media_dir_file_name.clone(),
        real_path: media_dir_path,
        symlink_path: path_accumulator.symlink_path.clone(),
        static_path: path_accumulator.static_path.clone(),
        thumbnail_url: None,
        library_id: library_ids.library_id.clone(),
        library_dir_id: library_ids.library_dir_id.clone(),
        show_id: show_id.cloned()
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return create_media_dir(pool_clone, new_media_dir);
      }).await;

      let media_dir_record = match block_thread_result
      {
        Ok(create_media_dir_result) => {
          match create_media_dir_result
          {
            Ok(media_dir_record) => { media_dir_record },
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

      media_dir = media_dir_record.clone();

      let media_dir_seen = MediaDirSeen {
        media_dir: media_dir_record,
        seen: true
      };

      media_seen.insert(direntry_id, media_dir_seen);
    }
  }

  return Ok(media_dir);
}


async fn check_seen_collection_movie(collection_movie_dir: &DirEntry,
  scan_movies_collection_info: &mut ScanMoviesCollectionInfo<'_>,
  movie: &MediaDir, pool: &web::Data<DBPool>) -> Result<String, MKError>
{
    let device_id_metadata = match mk_device_id_metadata(&collection_movie_dir) {
      Ok(metadata) => { metadata },
      Err(err) => { return Err(err); }
    };

    let device_id = device_id_metadata.dev().to_string();
    let ino = collection_movie_dir.ino().to_string();

    let direntry_id = DirEntryId {
      ino: ino.clone(),
      device_id: device_id.clone()
    };

    match scan_movies_collection_info.collection_movies_seen
      .get_mut(&direntry_id)
    {
      Some(collection_movie) => { collection_movie.seen = true },
      None => {
        let new_collection_movie = NewCollectionMovie {
          movie_id: movie.id.clone(),
          movie_path: movie.real_path.clone(),
          collection_id: scan_movies_collection_info.collection.id.clone(),
          collection_name: scan_movies_collection_info.collection.name.clone()
        };

        let pool_clone = pool.clone();
        let block_thread_result = web::block(move || {
          return create_collection_movie(pool_clone, new_collection_movie);
        }).await;

        let collection_movie_record = match block_thread_result
        {
          Ok(create_collection_movie_result) => {
            match create_collection_movie_result
            {
              Ok(collection_movie_record) => { collection_movie_record },
              Err(err) => { return Err(err); }
            }
          },
          Err(err) => { return Err(blocking_error(err)); }
        };

        let collection_movie_info = CollectionMovieInfo {
          id: collection_movie_record.id,
          collection_id: scan_movies_collection_info.collection.id.clone(),
          collection_name: scan_movies_collection_info.collection.name.clone(),
          movie_id: movie.id.clone(),
          ino: ino.clone(),
          device_id: device_id.clone(),
          movie_path: movie.real_path.clone()
        };

        let collection_movie_seen = CollectionMovieSeen {
          collection_movie: collection_movie_info,
          seen: true
        };

        scan_movies_collection_info.collection_movies_seen
          .insert(direntry_id, collection_movie_seen);
      }
    }

  return Ok("Successfully checked seen collection movie".to_string());
}

async fn check_seen_video(video: &DirEntry,
  recursion_state: &mut ScanMediaRecursionState<'_>,
  path_accumulator: &PathAccumulator, is_extras_dir: bool, is_proc_dir: bool,
  pool: &web::Data<DBPool>) -> Result<String, MKError>
{
  let video_file_name = match mk_pathbuf_file_name(&video.path()) {
    Ok(collection_name) => { collection_name },
    Err(err) => { return Err(err); }
  };

  let video_path = match mk_pathbuf_to_string(&video.path()) {
    Ok(path) => { path },
    Err(err) => { return Err(err); }
  };

  match recursion_state.videos_seen.get_mut(&video_file_name) {
    Some(video_seen) => {
      video_seen.seen = true;

      let static_path: Option<String>;
      let real_path: Option<String>;

      if is_proc_dir || !video_seen.processed {
        static_path = Some(path_accumulator.static_path.clone());
      }
      else {
        static_path = None;
      }

      if !is_proc_dir {
        real_path = Some(video_path.clone());
      }
      else {
        real_path = None;
      }

      if is_proc_dir { video_seen.processed = true };

      let update_video_info = UpdateVideo {
        id: video_seen.video.id.clone(),
        name: video_file_name,
        real_path: real_path,
        static_path: static_path,
        extra: is_extras_dir,
        processed: video_seen.processed
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return update_video(pool_clone, update_video_info);
      }).await;

      let _video_record = match block_thread_result
      {
        Ok(create_video_result) => {
          match create_video_result
          {
            Ok(video_record) => { video_record },
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };
    },
    None => {
      let new_video = NewVideo {
        name: video_file_name.clone(),
        real_path: video_path,
        static_path: path_accumulator.static_path.clone(),
        extra: is_extras_dir,
        processed: is_proc_dir,
        thumbnail_url: None,
        media_dir_id: recursion_state.media_dir_id.clone()
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return create_video(pool_clone, new_video);
      }).await;

      let video_record = match block_thread_result
      {
        Ok(create_video_result) => {
          match create_video_result
          {
            Ok(video_record) => { video_record },
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

      let video_seen = VideoSeen {
        video: video_record,
        seen: true,
        processed: is_proc_dir
      };

      recursion_state.videos_seen.insert(video_file_name, video_seen);
    }
  }

  return Ok("successfully checked seen video".to_string());
}

async fn scan_media_dir(media_dir: &DirEntry,
  recursion_state: & mut Option<& mut ScanMediaRecursionState<'_>>,
  path_accumulator: &mut PathAccumulator,
  media_seen: &mut MediaSeen,
  scan_movies_collection_info: &mut Option<&mut ScanMoviesCollectionInfo<'_>>,
  is_extras_dir: bool, is_proc_dir: bool, library_ids: &LibraryIds,
  show_id: Option<&String>, pool: &web::Data<DBPool>) -> Result<String, MKError>
{
  if media_dir.path().is_file() { return Ok("media dir is file.".to_string()); }

  let mut videos_seen: VideosSeen = HashMap::new();
  let mut media_dir_record = MediaDir::new();

  match recursion_state {
    Some(_) => {},
    None => {
      media_dir_record = match check_seen_media_dir(media_dir, path_accumulator,
        media_seen, library_ids, show_id, pool).await {
          Ok(id) => { id },
          Err(err) => { return Err(err); }
        };

      match scan_movies_collection_info {
        Some(some_scan_movies_collection_info) => {
          match check_seen_collection_movie(&media_dir,
            some_scan_movies_collection_info, &media_dir_record, pool).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        },
        None => {}
      }

      videos_seen = match get_initial_videos(&media_dir_record, pool).await {
        Ok(videos) => { videos },
        Err(err) => { return Err(err); }
      }
    }
  }

  let media_items = match mk_read_dir(&media_dir.path()) {
    Ok(media) => { media },
    Err(err) => { return Err(err); }
  };

  let mut media_item: DirEntry;
  let mut media_item_file_name: String;
  let mut path_accumulator_clone: PathAccumulator;
  let mut initial_recursion_state: ScanMediaRecursionState;

  for media_item_result in media_items
  {
    media_item = match media_item_result {
      Ok(media_item) => { media_item },
      Err(err) => { eprintln!("Error: {:?}", err); continue; }
    };

    media_item_file_name = match mk_pathbuf_file_name(&media_item.path()) {
      Ok(file_name) => { file_name },
      Err(err) => { return Err(err); }
    };

    if media_item_file_name == "clips" ||
      media_item_file_name == "test_encodings"
    { continue; }

    path_accumulator_clone = path_accumulator.clone();

    path_accumulator_clone.symlink_path = format!("{}/{}",
      &path_accumulator.symlink_path, &media_item_file_name);

    path_accumulator_clone.static_path = format!("{}/{}",
      &path_accumulator.static_path, &media_item_file_name);

    match recursion_state {
      Some(some_recursive_data) =>
      {
        if media_item.path().is_file()
        {
          match check_seen_video(&media_item, some_recursive_data,
            &path_accumulator_clone, is_extras_dir, is_proc_dir, pool).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        }
        else if media_item_file_name == "extras" && !is_extras_dir
        {
          match Box::pin(scan_media_dir(&media_item,
            &mut Some(some_recursive_data),
            &mut path_accumulator_clone, media_seen,
            scan_movies_collection_info, true, is_proc_dir,
            library_ids, show_id, pool)).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        }
        else if media_item_file_name == "proc" || is_proc_dir
        {
          match Box::pin(scan_media_dir(&media_item,
            &mut Some(some_recursive_data),
            &mut path_accumulator_clone, media_seen,
            scan_movies_collection_info, is_extras_dir, true,
            library_ids, show_id, pool)).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        }
      },
      None => {
        initial_recursion_state = ScanMediaRecursionState {
          media_dir_id: &media_dir_record.id,
          videos_seen: &mut videos_seen
        };

        if media_item.path().is_file()
        {
          match check_seen_video(&media_item, &mut initial_recursion_state,
            &mut path_accumulator_clone, is_extras_dir, is_proc_dir, pool).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        }
        else if media_item_file_name == "extras" && !is_extras_dir
        {
          match Box::pin(scan_media_dir(&media_item,
            &mut Some(&mut initial_recursion_state), &mut path_accumulator_clone,
            media_seen, scan_movies_collection_info, true, is_proc_dir,
            library_ids, show_id, pool)).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        }
        else if media_item_file_name == "proc" || is_proc_dir
        {
          match Box::pin(scan_media_dir(&media_item,
            &mut Some(&mut initial_recursion_state), &mut path_accumulator_clone,
            media_seen, scan_movies_collection_info, is_extras_dir, true,
            library_ids, show_id, pool)).await
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        }
      }
    }
  }

  let mut pool_clone: web::Data<DBPool>;
  let mut video: Video;

  for video_seen in videos_seen {
    if video_seen.1.seen == false
    {
      pool_clone = pool.clone();
      video = video_seen.1.video;

      let block_thread_result = web::block(move || {
        return delete_video(pool_clone, video);
      }).await;

      match block_thread_result
      {
        Ok(delete_video_result) => {
          match delete_video_result {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };
    }
  }

  return Ok("successfully scanned media dir".to_string());
}

async fn check_seen_collection(collection_dir: &DirEntry,
  collections_seen: &mut CollectionsSeen, library_ids: &LibraryIds,
  pool: &web::Data<DBPool>) -> Result<Collection, MKError>
{
  let device_id_metadata = match mk_device_id_metadata(&collection_dir) {
    Ok(metadata) => { metadata },
    Err(err) => { return Err(err); }
  };

  let device_id = device_id_metadata.dev().to_string();
  let ino = collection_dir.ino().to_string();

  let collection_name = match mk_pathbuf_file_name(&collection_dir.path()) {
    Ok(collection_name) => { collection_name },
    Err(err) => { return Err(err); }
  };

  let direntry_id = DirEntryId {
    ino: ino.clone(),
    device_id: device_id.clone()
  };

  let collection: Collection;

  match collections_seen.get_mut(&direntry_id) {
    Some(collection_seen) => {

      collection_seen.seen = true;
      collection = collection_seen.collection.clone();

      let update_collection_info = UpdateCollection {
        id: collection.id.clone(),
        name: collection_name
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return update_collection(pool_clone, update_collection_info);
      }).await;

      match block_thread_result
      {
        Ok(update_collection_result) => {
          match update_collection_result
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

    },
    None => {
      let new_collection = NewCollection {
        name: collection_name.clone(),
        ino: Some(ino.clone()),
        device_id: Some(device_id.clone()),
        library_id: Some(library_ids.library_id.clone())
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return create_collection(pool_clone, new_collection);
      }).await;

      let collection_record = match block_thread_result
      {
        Ok(create_collection_result) => {
          match create_collection_result
          {
            Ok(collection_record) => { collection_record },
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

      let collection_seen = CollectionSeen {
        collection: collection_record.clone(),
        seen: true
      };

      collections_seen.insert(direntry_id, collection_seen);

      collection = collection_record;
    }
  }

  return Ok(collection);
}

async fn scan_collections(collections_dir: &DirEntry,
  path_accumulator: &PathAccumulator, collections_seen: &mut CollectionsSeen,
  movies_seen: &mut Option<&mut MediaSeen>, shows_seen: &mut Option<&mut ShowsSeen>,
  library_ids: &LibraryIds, pool: &web::Data<DBPool>) -> Result<String, MKError>
{

  let collection_dirs = match mk_read_dir(&collections_dir.path()) {
    Ok(_collection_dirs) => { _collection_dirs },
    Err(err) => { return Err(err); }
  };

  let mut collection_dir: DirEntry;
  let mut collection: Collection;
  let mut collection_name: String;
  let mut path_accumulator_clone: PathAccumulator;
  let mut collection_shows_seen: CollectionShowsSeen;
  let mut scan_shows_collection_info: ScanShowsCollectionInfo;
  let mut collection_movies_seen: CollectionMoviesSeen;
  let mut scan_movies_collection_info: ScanMoviesCollectionInfo;
  let mut pool_clone: web::Data<DBPool>;
  let mut delete_collection_show_info: DeleteCollectionShow;
  let mut delete_collection_movie_info: DeleteCollectionMovie;

  for collection_dir_result in collection_dirs
  {
    collection_dir = match collection_dir_result {
      Ok(collection_dir) => { collection_dir },
      Err(err) => { eprintln!("Error: {:?}", err); continue; }
    };

    if collection_dir.path().is_file() { continue; }

    collection = match check_seen_collection(&collection_dir,
      collections_seen, library_ids, pool).await {
        Ok(collection) => { collection },
        Err(err) => { return Err(err); }
      };

    collection_name = match mk_pathbuf_file_name(&collection_dir.path()) {
      Ok(collection_name) => { collection_name },
      Err(err) => { return Err(err); }
    };

    path_accumulator_clone = path_accumulator.clone();

    path_accumulator_clone.symlink_path = format!("{}/{}",
      &path_accumulator.symlink_path, &collection_name);

    path_accumulator_clone.static_path = format!("{}/{}",
      &path_accumulator.static_path, &collection_name);

    match shows_seen {
      Some(some_shows_seen) => {
        collection_shows_seen =
          match get_initial_collection_shows(&collection, pool).await
          {
            Ok(collection_shows_seen) => { collection_shows_seen },
            Err(err) => { return Err(err); }
          };

        scan_shows_collection_info = ScanShowsCollectionInfo {
          collection_shows_seen: &mut collection_shows_seen,
          collection: &collection
        };

        match Box::pin(scan_shows(&collection_dir.path(),
          &mut path_accumulator_clone, some_shows_seen,
          collections_seen,
          &mut Some(&mut scan_shows_collection_info),
          library_ids, pool)).await
        {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }

        for collection_show_seen in collection_shows_seen {
          if collection_show_seen.1.seen == false
          {
            pool_clone = pool.clone();

            delete_collection_show_info = DeleteCollectionShow {
              id: collection_show_seen.1.collection_show.id,
              show_id: collection_show_seen.1.collection_show.show_id,
              show_path: collection_show_seen.1.collection_show.show_path,
              collection_id: collection_show_seen.1.collection_show.collection_id,
              collection_name: collection_show_seen.1.collection_show.collection_name
            };

            let block_thread_result = web::block(move || {
              return delete_collection_show(pool_clone, delete_collection_show_info);
            }).await;

            match block_thread_result
            {
              Ok(delete_collection_show_result) => {
                match delete_collection_show_result {
                  Ok(_) => {},
                  Err(err) => { return Err(err); }
                }
              },
              Err(err) => { return Err(blocking_error(err)); }
            };
          }
        }
      },
      None => {}
    }

    match movies_seen {
      Some(some_movies_seen) => {
        collection_movies_seen =
          match get_initial_collection_movies(&collection, pool).await
          {
            Ok(collection_movies_seen) => { collection_movies_seen },
            Err(err) => { return Err(err); }
          };

        scan_movies_collection_info = ScanMoviesCollectionInfo {
          collection_movies_seen: &mut collection_movies_seen,
          collection: &collection
        };

        match Box::pin(scan_movies(&collection_dir.path(),
          &mut path_accumulator_clone, some_movies_seen, collections_seen,
          &mut Some(&mut scan_movies_collection_info),
          library_ids, pool)).await
        {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }

        for collection_movie_seen in collection_movies_seen {
          if collection_movie_seen.1.seen == false
          {
            pool_clone = pool.clone();

            delete_collection_movie_info = DeleteCollectionMovie {
              id: collection_movie_seen.1.collection_movie.id,
              movie_id: collection_movie_seen.1.collection_movie.movie_id,
              movie_path: collection_movie_seen.1.collection_movie.movie_path,
              collection_id: collection_movie_seen.1.collection_movie.collection_id,
              collection_name: collection_movie_seen.1.collection_movie.collection_name
            };

            let block_thread_result = web::block(move || {
              return delete_collection_movie(pool_clone, delete_collection_movie_info);
            }).await;

            match block_thread_result
            {
              Ok(delete_collection_movie_result) => {
                match delete_collection_movie_result {
                  Ok(_) => {},
                  Err(err) => { return Err(err); }
                }
              },
              Err(err) => { return Err(blocking_error(err)); }
            };
          }
        }
      },
      None => {}
    }
  }

  return Ok("Successfully scanned collections".to_string());
}

async fn scan_movies(scan_pathbuf: &PathBuf,
  path_accumulator: &mut PathAccumulator,
  movies_seen: &mut MediaSeen,
  collections_seen: &mut CollectionsSeen,
  scan_movies_collection_info: &mut Option<&mut ScanMoviesCollectionInfo<'_>>,
  library_ids: &LibraryIds, pool: &web::Data<DBPool>) -> Result<String, MKError>
{
  let movie_dirs = match mk_read_dir(scan_pathbuf) {
    Ok(movie_dirs) => { movie_dirs },
    Err(err) => { return Err(err); }
  };

  let mut movie_dir: DirEntry;
  let mut movie_dir_file_name: String;
  let mut path_accumulator_clone: PathAccumulator;

  for movie_dir_result in movie_dirs
  {
    movie_dir = match movie_dir_result {
      Ok(movie_dir) => { movie_dir },
      Err(err) => { eprintln!("Error: {:?}", err); continue; }
    };

    movie_dir_file_name = match mk_pathbuf_file_name(&movie_dir.path()) {
      Ok(file_name) => { file_name },
      Err(err) => { return Err(err); }
    };

    path_accumulator_clone = path_accumulator.clone();

    path_accumulator_clone.symlink_path = format!("{}/{}",
      &path_accumulator.symlink_path, &movie_dir_file_name);

    path_accumulator_clone.static_path = format!("{}/{}",
      &path_accumulator.static_path, &movie_dir_file_name);

    match scan_movies_collection_info {
      Some(_) => {
        match scan_media_dir(&movie_dir, &mut None, &mut path_accumulator_clone,
          movies_seen, scan_movies_collection_info, false, false, library_ids,
          None, pool).await
        {
          Ok(_) => { continue; },
          Err(err) => { return Err(err); }
        }
      },
      None => {
        if  movie_dir_file_name.to_ascii_lowercase() == "collections"
        {
          match scan_collections(&movie_dir, &path_accumulator_clone,
            collections_seen, &mut Some(movies_seen), &mut None, library_ids,
            pool).await
          {
            Ok(_) => { continue; },
            Err(err) => { return Err(err); }
          }
        }

        match scan_media_dir(&movie_dir, &mut None, &mut path_accumulator_clone,
          movies_seen, &mut None, false, false, library_ids, None, pool).await
        {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }
      }
    }

  }

  return Ok("successfully scanned library dir".to_string());
}

async fn check_seen_show(show_dir: &DirEntry,
  path_accumulator: &PathAccumulator, library_ids: &LibraryIds,
  shows_seen: &mut ShowsSeen,
  pool: &web::Data<DBPool>)
  -> Result<Show, MKError>
{
  let device_id_metadata = match mk_device_id_metadata(&show_dir) {
    Ok(metadata) => { metadata },
    Err(err) => { return Err(err); }
  };

  let device_id = device_id_metadata.dev().to_string();
  let ino = show_dir.ino().to_string();

  let show_dir_file_name = match mk_pathbuf_file_name(&show_dir.path()) {
    Ok(file_name) => { file_name },
    Err(err) => { return Err(err); }
  };

  let show_dir_path = match mk_pathbuf_to_string(&show_dir.path()) {
    Ok(path) => { path },
    Err(err) => { return Err(err); }
  };

  let direntry_id = DirEntryId {
    ino: ino.clone(),
    device_id: device_id.clone()
  };

  let show: Show;

  match shows_seen.get_mut(&direntry_id) {
    Some(show_seen) => {
      show_seen.seen = true;
      show = show_seen.show.clone();

      let update_show_info = UpdateShow {
        id: show.id.clone(),
        name: show_dir_file_name,
        real_path: show_dir_path,
        symlink_path: path_accumulator.symlink_path.clone(),
        static_path: path_accumulator.static_path.clone(),
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return update_show(pool_clone, update_show_info);
      }).await;

      match block_thread_result
      {
        Ok(update_show_result) => {
          match update_show_result
          {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

    },
    None => {
      let new_show = NewShow {
        ino: ino.clone(),
        device_id: device_id.clone(),
        name: show_dir_file_name.clone(),
        real_path: show_dir_path,
        symlink_path: path_accumulator.symlink_path.clone(),
        static_path: path_accumulator.static_path.clone(),
        library_id: library_ids.library_id.clone(),
        library_dir_id: library_ids.library_dir_id.clone()
      };

      let pool_clone = pool.clone();
      let block_thread_result = web::block(move || {
        return create_show(pool_clone, new_show);
      }).await;

      let show_record = match block_thread_result
      {
        Ok(create_show_result) => {
          match create_show_result
          {
            Ok(show_record) => { show_record },
            Err(err) => { return Err(err); }
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };

      show = show_record.clone();

      let show_seen = ShowSeen {
        show: show_record,
        seen: true
      };

      shows_seen.insert(direntry_id, show_seen);
    }
  }

  return Ok(show);
}

async fn check_seen_collection_show(collection_show_dir: &DirEntry,
  scan_shows_collection_info: &mut ScanShowsCollectionInfo<'_>,
  show: &Show, pool: &web::Data<DBPool>)
  -> Result<String, MKError>
{
    let device_id_metadata = match mk_device_id_metadata(&collection_show_dir) {
      Ok(metadata) => { metadata },
      Err(err) => { return Err(err); }
    };

    let device_id = device_id_metadata.dev().to_string();
    let ino = collection_show_dir.ino().to_string();

    let direntry_id = DirEntryId {
      ino: ino.clone(),
      device_id: device_id.clone()
    };

    match scan_shows_collection_info.collection_shows_seen
      .get_mut(&direntry_id)
    {
      Some(collection_show) => { collection_show.seen = true },
      None => {
        let new_collection_show = NewCollectionShow {
          show_id: show.id.clone(),
          show_path: show.real_path.clone(),
          collection_id: scan_shows_collection_info.collection.id.clone(),
          collection_name: scan_shows_collection_info.collection.name.clone()
        };

        let pool_clone = pool.clone();
        let block_thread_result = web::block(move || {
          return create_collection_show(pool_clone,new_collection_show);
        }).await;

        let collection_show_record = match block_thread_result
        {
          Ok(create_collection_show_result) => {
            match create_collection_show_result
            {
              Ok(collection_show_record) => { collection_show_record },
              Err(err) => { return Err(err); }
            }
          },
          Err(err) => { return Err(blocking_error(err)); }
        };

        let collection_show_info = CollectionShowInfo {
          id: collection_show_record.id,
          collection_id: scan_shows_collection_info.collection.id.clone(),
          collection_name: scan_shows_collection_info.collection.name.clone(),
          show_id: show.id.clone(),
          ino: ino,
          device_id: device_id,
          show_path: show.real_path.clone()
        };

        let collection_show_seen = CollectionShowSeen {
          collection_show: collection_show_info,
          seen: true
        };

        scan_shows_collection_info.collection_shows_seen
          .insert(direntry_id, collection_show_seen);
      }
    }

  return Ok("Successfully checked seen collection show".to_string());
}

async fn scan_shows(scan_path_buf: &PathBuf,
  path_accumulator: &mut PathAccumulator,
  shows_seen: &mut ShowsSeen,
  collections_seen: &mut CollectionsSeen,
  scan_shows_collection_info: &mut Option<&mut ScanShowsCollectionInfo<'_>>,
  library_ids: &LibraryIds, pool: &web::Data<DBPool>) -> Result<String, MKError>
{
  let show_dirs = match mk_read_dir(scan_path_buf) {
    Ok(show_dirs) => { show_dirs },
    Err(err) => { return Err(err); }
  };

  let mut show_dir: DirEntry;
  let mut show_dir_file_name: String;
  let mut show: Show;
  let mut path_accumulator_show_clone: PathAccumulator;
  let mut season_dir_file_name: String;
  let mut path_accumulator_season_clone: PathAccumulator;
  let mut seasons_seen: MediaSeen;
  let mut season_dirs: ReadDir;
  let mut season_dir: DirEntry;
  let mut pool_clone: web::Data<DBPool>;
  let mut season: MediaDir;

  for show_dir_result in show_dirs
  {
    show_dir = match show_dir_result {
      Ok(show_dir) => { show_dir },
      Err(err) => { eprintln!("Error: {:?}", err); continue; }
    };

    if show_dir.path().is_file() { continue; }

    show_dir_file_name = match mk_pathbuf_file_name(&show_dir.path()) {
      Ok(file_name) => { file_name },
      Err(err) => { return Err(err); }
    };
    
    path_accumulator_show_clone = path_accumulator.clone();

    path_accumulator_show_clone.symlink_path = format!("{}/{}",
      &path_accumulator.symlink_path, &show_dir_file_name);

    path_accumulator_show_clone.static_path = format!("{}/{}",
      &path_accumulator.static_path, &show_dir_file_name);

    match scan_shows_collection_info {
      Some(_) => {},
      None => {
        if show_dir_file_name.to_ascii_lowercase() == "collections"
        {
          match scan_collections(&show_dir, &path_accumulator_show_clone,
            collections_seen, &mut None, &mut Some(shows_seen), library_ids,
            pool).await
          {
            Ok(_) => { continue; },
            Err(err) => { return Err(err); }
          }
        }
      }
    }

    show = match check_seen_show(&show_dir, &path_accumulator_show_clone,
      library_ids, shows_seen, pool).await {
        Ok(show) => { show },
        Err(err) => { return Err(err); }
      };

    match scan_shows_collection_info {
      Some(some_scan_shows_collection_info) => {
        match check_seen_collection_show(&show_dir, some_scan_shows_collection_info,
          &show, pool).await {
            Ok(_) => {},
            Err(err) => { return Err(err); }
          }
      },
      None => {}
    }

    seasons_seen = match get_initial_seasons(&show, pool).await {
      Ok(seasons_seen) => { seasons_seen },
      Err(err) => { return Err(err); }
    };

    season_dirs = match mk_read_dir(&show_dir.path()) {
      Ok(season_dirs) => { season_dirs },
      Err(err) => { return Err(err); }
    };

    for season_dir_result in season_dirs
    {
      season_dir = match season_dir_result {
        Ok(season_dir) => { season_dir },
        Err(err) => { eprintln!("Error: {:?}", err); continue; }
      };

      if season_dir.path().is_file() { continue; }

      season_dir_file_name = match mk_pathbuf_file_name(&season_dir.path()) {
        Ok(file_name) => { file_name },
        Err(err) => { return Err(err); }
      };

      path_accumulator_season_clone = path_accumulator_show_clone.clone();

      path_accumulator_season_clone.symlink_path = format!("{}/{}",
        &path_accumulator_show_clone.symlink_path, &season_dir_file_name);

      path_accumulator_season_clone.static_path = format!("{}/{}",
        &path_accumulator_show_clone.static_path, &season_dir_file_name);

      if season_dir.file_name().to_ascii_lowercase() == "extras"
      {
        match scan_media_dir(&season_dir, &mut None,
          &mut path_accumulator_season_clone, &mut seasons_seen, &mut None,
          true, false, library_ids, Some(&show.id), pool).await
        {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }
      }
      else {
        match scan_media_dir(&season_dir, &mut None,
          &mut path_accumulator_season_clone, &mut seasons_seen, &mut None,
          false, false, library_ids, Some(&show.id), pool).await
        {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }
      }
    }

    for season_seen in seasons_seen {
      if season_seen.1.seen == false {
        pool_clone = pool.clone();
        season = season_seen.1.media_dir;

        let block_thread_result = web::block(move || {
          return delete_media_dir(pool_clone, season);
        }).await;

        match block_thread_result
        {
          Ok(delete_season_result) => {
            match delete_season_result {
              Ok(_) => {},
              Err(err) => { return Err(err); }
            }
          },
          Err(err) => { return Err(blocking_error(err)); }
        };
      }
    }
  }

  return Ok("successfully scanned shows library dir".to_string());
}

#[post("/scan_library")]
pub async fn scan_library(scan_library_info: web::Json<ScanLibraryInfo>,
  pool: web::Data<DBPool>) -> actix_web::Result<String>
{
  let library_record =
    match get_library_record(&scan_library_info.library_id, &pool).await
    {
      Ok(library_record) => { library_record },
      Err(err) => { return Err(err.into()); }
    };

  let library_dir_records =
    match get_library_dir_records(&library_record, &pool).await
    {
      Ok(library_dir_records) => { library_dir_records },
      Err(err) => { return Err(err.into()); }
    };

  let mut collections_seen =
    match get_initial_collections(&library_record, &pool).await
    {
      Ok(collections_seen) => { collections_seen },
      Err(err) => { return Err(err.into()); }
    };

  let mut path_accumulator: PathAccumulator;
  let mut library_dir_pathbuf: PathBuf;
  let mut library_ids: LibraryIds;
  let mut pool_clone: web::Data<DBPool>;
  let mut movie: MediaDir;
  let mut show: Show;
  let mut collection: Collection;

  if library_record.media_type == MediaType::Movie.to_string()
  {
    let mut movies_seen =
      match get_initial_movies(&library_record, &pool).await
      {
        Ok(movies_seen) => { movies_seen },
        Err(err) => { return Err(err.into()); }
      };

    for library_dir_record in library_dir_records
    {
      library_dir_pathbuf = PathBuf::from(&library_dir_record.real_path);

      path_accumulator = PathAccumulator {
        symlink_path: library_dir_record.symlink_path.clone(),
        static_path: library_dir_record.static_path.clone()
      };

      library_ids = LibraryIds {
        library_id: library_record.id.clone(),
        library_dir_id: library_dir_record.id.clone()
      };

      match scan_movies(&library_dir_pathbuf, &mut path_accumulator,
        &mut movies_seen, &mut collections_seen, &mut None,
        &library_ids, &pool).await
      {
        Ok(_) => {},
        Err(err) => { return Err(err.into()); }
      }
    }

    for movie_seen in movies_seen {
      if movie_seen.1.seen == false {
         pool_clone = pool.clone();
         movie = movie_seen.1.media_dir;

        let block_thread_result = web::block(move || {
          return delete_media_dir(pool_clone, movie);
        }).await;

        match block_thread_result
        {
          Ok(delete_collection_video_result) => {
            match delete_collection_video_result {
              Ok(_) => {},
              Err(err) => { return Err(err.into()); }
            }
          },
          Err(err) => { return Err(blocking_error(err).into()); }
        };
      }
    }
  }
  else if library_record.media_type == MediaType::Show.to_string()
  {
    let mut shows_seen =
      match get_initial_shows(&library_record, &pool).await
      {
        Ok(media_seen) => { media_seen },
        Err(err) => { return Err(err.into()); }
      };

    for library_dir_record in library_dir_records
    {
      library_dir_pathbuf = PathBuf::from(&library_dir_record.real_path);

      path_accumulator = PathAccumulator {
        symlink_path: library_dir_record.symlink_path.clone(),
        static_path: library_dir_record.static_path.clone()
      };

      library_ids = LibraryIds {
        library_id: library_record.id.clone(),
        library_dir_id: library_dir_record.id.clone()
      };

      match scan_shows(&library_dir_pathbuf, &mut path_accumulator,
        &mut shows_seen, &mut collections_seen, &mut None,
        &library_ids, &pool).await
      {
        Ok(_) => {},
        Err(err) => { return Err(err.into()); }
      }
    }

    for show_seen in shows_seen {
      if show_seen.1.seen == false {
         pool_clone = pool.clone();
         show = show_seen.1.show;

        let block_thread_result = web::block(move || {
          return delete_show(pool_clone, show);
        }).await;

        match block_thread_result
        {
          Ok(delete_collection_video_result) => {
            match delete_collection_video_result {
              Ok(_) => {},
              Err(err) => { return Err(err.into()); }
            }
          },
          Err(err) => { return Err(blocking_error(err).into()); }
        };
      }
    }
  }

  for collection_seen in collections_seen {
    if collection_seen.1.seen == false {
      pool_clone = pool.clone();
      collection = collection_seen.1.collection;

      let block_thread_result = web::block(move || {
        return delete_collection(pool_clone, collection);
      }).await;

      match block_thread_result
      {
        Ok(delete_collection_result) => {
          match delete_collection_result {
            Ok(_) => {},
            Err(err) => { return Err(err.into()); }
          }
        },
        Err(err) => { return Err(blocking_error(err).into()); }
      };
    }
  }

  return Ok(format!("successfully scanned library {}", &library_record.name));
}
