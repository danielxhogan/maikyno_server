use crate::utils::mk_error::{MKError, MKErrorType};

use std::{
  fs::{create_dir_all, read_dir, remove_dir_all, DirEntry, Metadata, ReadDir},
  path::PathBuf
};

pub fn mk_create_dir_all(dir_path: &String) -> Result<(), MKError>
{
  match create_dir_all(dir_path) {
    Ok(_) => Ok(()),
    Err(err) => {
      let err_msg = format!("{:?}: {:?}\nError: {:?}",
        MKErrorType::CreateDirAllError.to_string(), &dir_path, err);

      println!("{err_msg}");
      return Err(MKError::new(MKErrorType::CreateDirAllError, err_msg));
    }
  }
}

pub fn mk_remove_dir_all(dir_path: &String) -> Result<(), MKError>
{
  match remove_dir_all(dir_path) {
    Ok(_) => Ok(()),
    Err(err) => {
      let err_msg = format!("{:?}: {:?}\nError: {:?}",
        MKErrorType::RemoveDirAllError.to_string(), &dir_path, err);

      println!("{err_msg}");
      return Err(MKError::new(MKErrorType::RemoveDirAllError, err_msg));
    }
  }
}

pub fn mk_read_dir(dir_path: &PathBuf)
  -> Result<ReadDir, MKError>
{
  let dirs = match read_dir(dir_path)
  {
    Ok(dirs) => { dirs },
    Err(err) => {
      let err_msg = format!("{:?}: {:?}\nError: {:?}",
        MKErrorType::ReadDirError.to_string(),
        dir_path, err);

      eprintln!("{err_msg:?}");
      return Err(MKError::new(
        MKErrorType::ReadDirError, err_msg));
    }
  };

  return Ok(dirs);
}

pub fn mk_pathbuf_to_string(path: &PathBuf) -> Result<String, MKError>
{
  match path.to_str()
  {
    Some(path_str) => {
      return Ok(path_str.to_string());
    },
    None => {
      let err_msg = format!("{:?}",
        MKErrorType::PathBufStringError.to_string());

      eprintln!("{err_msg}");
      return Err(MKError::new(MKErrorType::PathBufStringError, err_msg));
    }
  }
}

pub fn mk_pathbuf_file_name(path: &PathBuf) -> Result<String, MKError>
{
  match path.file_name() {
    Some(path_os_str) => {
      match path_os_str.to_str()
      {
        Some(path_file_name) => {
          return Ok(path_file_name.to_string());
        },
        None => {
          let err_msg = format!("{:?}",
            MKErrorType::FileNameError.to_string());

          eprintln!("{err_msg}");
          return Err(MKError::new(MKErrorType::FileNameError, err_msg));
        }
      }
    },
    None => {
      let err_msg = format!("{:?}",
        MKErrorType::FileNameError.to_string());

      eprintln!("{err_msg}");
      return Err(MKError::new(MKErrorType::FileNameError, err_msg));
    }
  }
}

pub fn mk_device_id_metadata(dir_entry: &DirEntry)
  -> Result<Metadata, MKError>
{
  let metadata = match dir_entry.metadata() {
    Ok(metadata) => { metadata },
    Err(err) => {
      let err_msg = format!("{:?}: {:?}\nError: {:?}",
        MKErrorType::DirEntryMetadataError.to_string(),
        dir_entry.path(), err);

      eprintln!("{err_msg}");
      return Err(MKError::new(
        MKErrorType::DirEntryMetadataError, err_msg).into());
    }
  };

  return Ok(metadata);
}
