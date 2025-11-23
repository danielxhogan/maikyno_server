use crate::db::{
  config::{
    db_connect::DBPool,
    models::{
      NewLibrary,
      NewLibraryDir
    }
  },
  library::{
    create_library, MediaType
  },
};

use crate::AppState;

use crate::utils::{
  mk_error::{MKError, MKErrorType, blocking_error},
  mk_fs::{mk_create_dir_all, mk_remove_dir_all, mk_read_dir}
};

use actix_web::{post, web};
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

  for new_library_info_path in new_library_info.paths.clone() {
    ino_opt = None;
    device_id_opt = None;
    new_library_path = PathBuf::from(&new_library_info_path);

    if !new_library_path.exists() {
      err_msg = format!("{:?}: {:?}",
        MKErrorType::LibraryPathNotFoundError.to_string(), &new_library_path);

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

    new_library_parent = match new_library_path.parent()
    {
      Some(new_library_parent) => { new_library_parent },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::ParentDirError.to_string(), &new_library_path);

        match mk_remove_dir_all(&root_library_dir) {
          Ok(_) => {},
          Err(err) => {
            err_msg = format!("{:?}\n{:?}", err_msg, err.message);
          }
        }

        println!("{err_msg}");
        return Err(MKError::new(MKErrorType::ParentDirError, err_msg).into());
      }
    };

    new_library_parent_dirs =
      match mk_read_dir(&new_library_parent.to_path_buf())
      {
        Ok(new_library_parent_dirs) => { new_library_parent_dirs },
        Err(err) => {
          match mk_remove_dir_all(&root_library_dir)
          {
            Ok(_) => {},
            Err(rm_dir_err) => {
              err_msg = format!("{:?}\n{:?}", err.message, rm_dir_err.message);
              eprintln!("{err_msg}");
              return Err(MKError::new(err.err_type, err_msg).into());
            }
          }

          return Err(err.into()); }
      };

    new_library_dir_name = match &new_library_path.file_name() {
      Some(new_library_dir_name) => { new_library_dir_name },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::FileNameError.to_string(), &new_library_path);

        match mk_remove_dir_all(&root_library_dir) {
          Ok(_) => {},
          Err(err) => {
            err_msg = format!("{:?}\n{:?}", err_msg, err.message);
          }
        }

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

        device_id_metadata = match dir_entry.metadata() {
          Ok(device_id_metadata) => { device_id_metadata },
          Err(err) => {
            err_msg = format!("{:?}: {:?}\nError: {:?}",
              MKErrorType::DirEntryMetadataError.to_string(),
              dir_entry.path(), err);

            match mk_remove_dir_all(&root_library_dir) {
              Ok(_) => {},
              Err(err) => {
                err_msg = format!("{:?}\n{:?}", err_msg, err.message);
              }
            }

            eprintln!("{err_msg}");
            return Err(MKError::new(
              MKErrorType::DirEntryMetadataError, err_msg).into());
          }
        };

        device_id_opt = Some(device_id_metadata.dev().to_string());
        break;
      }
    }

    ino = match ino_opt {
      Some(ino) => { ino },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::LibraryPathNotFoundError.to_string(), &new_library_path);

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

    device_id = match device_id_opt {
      Some(device_id) => { device_id },
      None => {
        err_msg = format!("{:?}: {:?}",
          MKErrorType::LibraryPathNotFoundError.to_string(), &new_library_path);

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

    symlink_path = format!("{}/{}",
      root_library_dir, new_library_dir_name.to_string_lossy());

    static_path = format!("{}/{}",
      new_library_info.name, &new_library_dir_name.to_string_lossy());

    new_library_dir = NewLibraryDir {
      name: new_library_dir_name.to_string_lossy().to_string(),
      real_path: new_library_info_path.clone(),
      symlink_path: symlink_path.clone(),
      static_path: static_path.clone(),
      ino: ino,
      device_id: device_id,
    };

    new_library_dirs.push(new_library_dir);

    #[cfg(target_family = "unix")]
    match std::os::unix::fs::symlink(&new_library_info_path, &symlink_path) {
      Ok(_) => {},
      Err(err) => {
        err_msg = format!("{:?}: {:?}\nPath: {:?}\nError: {:?}",
          MKErrorType::LibrarySymlinkError.to_string(),
          symlink_path, &new_library_info_path, err);

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
    };
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
