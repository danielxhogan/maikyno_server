use crate::db::{
  config::db_connect::DBPool,
  media::select_media_dir,
  proc::{create_batch, update_batch_abort}
};

use crate::utils::{
  proc::ProcessMediaInfo,
  mk_fs::mk_read_dir,
  mk_error::{MKError, MKErrorType, blocking_error},
};

use crate::av;

use actix_web::{post, web};
use serde::Deserialize;

use std::{
  ffi::CString,
  fs::{create_dir_all, rename},
  os::raw::{c_char},
  path::PathBuf
};

#[derive(Deserialize)]
struct RenameExtrasInfo {
  media_dir_id: String
}

#[derive(Deserialize, Clone)]
struct ScanMediaDirStreamsInfo {
  media_dir_id: String
}

#[derive(Deserialize)]
struct AbortBatchInfo {
  batch_id: String
}

#[post("/rename_extras")]
pub async fn rename_extras(rename_extras_info: web::Json<RenameExtrasInfo>,
  pool: web::Data<DBPool>) -> actix_web::Result<String>
{
  let pool_clone = pool.clone();
  let media_dir_id_clone = rename_extras_info.media_dir_id.clone();

  let block_thread_result = web::block(|| {
    return select_media_dir(pool_clone, media_dir_id_clone);
  }).await;

  let media_dir = match block_thread_result
  {
    Ok(select_media_dir_result) => {
      match select_media_dir_result {
        Ok(media_dir) => { media_dir },
        Err(err) => { return Err(err.into()); }
      }
    },
    Err(err) => { return Err(blocking_error(err).into()); }
  };

  let media_dir_extras_path = format!("{}/extras", media_dir.real_path);
  let extras_path_buf = PathBuf::from(&media_dir_extras_path);

  let extras = match mk_read_dir(&extras_path_buf) {
    Ok(extras) => { extras },
    Err(err) => { return Err(err.into()); }
  };

  let mut video_count = 1;

  for extra_result in extras {
    let extra = match extra_result {
      Ok(extra) => { extra },
      Err(err) => { eprintln!("Error geting extra dir entry: {:?}", err); continue; }
    };

    let extra_path = extra.path();

    let extra_parent = match extra_path.parent() {
      Some(file_stem) => { file_stem },
      None => { eprintln!("No parent found for extra {video_count}"); continue; }
    };

    let extra_ext = match extra_path.extension() {
      Some(ext) => { ext },
      None => { eprintln!("No extension found for extra {video_count}"); continue; }
    };

    let tmp_name = format!("{}/{:05}.{}",
      extra_parent.display(), video_count, extra_ext.display());

    match rename(extra_path, tmp_name) {
      Ok(_) => {},
      Err(err) => { eprintln!("Rename error: {:?}", err); continue; }
    }

    video_count += 1;
  }

  video_count = 1;

  let extras = match mk_read_dir(&extras_path_buf) {
    Ok(extras) => { extras },
    Err(err) => { return Err(err.into()); }
  };

  for extra_result in extras {
    let extra = match extra_result {
      Ok(extra) => { extra },
      Err(err) => { eprintln!("Error geting extra dir entry: {:?}", err); continue; }
    };

    let extra_path = extra.path();

    let extra_parent = match extra_path.parent() {
      Some(file_stem) => { file_stem },
      None => { eprintln!("No parent found for extra {video_count}"); continue; }
    };

    let extra_ext = match extra_path.extension() {
      Some(ext) => { ext },
      None => { eprintln!("No extension found for extra {video_count}"); continue; }
    };

    let new_name = format!("{}/{:02}.{}",
      extra_parent.display(), video_count, extra_ext.display());

    match rename(extra_path, new_name) {
      Ok(_) => {},
      Err(err) => { eprintln!("Rename error: {:?}", err); continue; }
    }

    video_count += 1;
  }

  return Ok("Successfully renamed extras".to_string());
}

#[post("/scan_media_streams")]
async fn scan_media_streams(
  scan_media_dir_streams_info: web::Json<ScanMediaDirStreamsInfo>)
  -> actix_web::Result<String>
{
  unsafe {
    let media_dir_id_c_str =
      match CString::new(scan_media_dir_streams_info.media_dir_id.clone()) {
        Ok(media_dir_id_c_str) => { media_dir_id_c_str },
        Err(err) => {
          let err_msg = format!("{:?}\nError: {:?}",
            MKErrorType::RustToCTypeConversionError.to_string(), err);

          eprintln!("{err_msg}");
          return Err(MKError::new(MKErrorType::RustToCTypeConversionError,
            err_msg).into());
        }
      };

    let media_dir_id_c_char: *const c_char = media_dir_id_c_str.as_ptr();
    av::scan_media_streams(media_dir_id_c_char);
  }

  return Ok("hello".to_string());
}

#[post("/process_media")]
pub async fn process_media(process_media_info: web::Json<ProcessMediaInfo>,
  pool: web::Data<DBPool>) -> actix_web::Result<String>
{
  if process_media_info.videos.len() == 0 {
    let err_msg = format!("No videos specified for media_dir: {:?}",
      process_media_info.media_dir_id);

    eprintln!("{err_msg}");
    return Err(MKError::new(MKErrorType::NoData, err_msg).into());
  }

  let pool_clone = pool.clone();
  let process_media_info_clone = process_media_info.clone();

  let block_thread_result = web::block(move || {
    return create_batch(process_media_info_clone, pool_clone);
  }).await;

  let batch = match block_thread_result
  {
    Ok(create_batch_result) => {
      match create_batch_result.await {
        Ok(batch) => { batch },
        Err(err) => { return Err(err.into()); }
      }
    },
    Err(err) => { return Err(blocking_error(err).into()); }
  };

  let pool_clone = pool.clone();
  let media_dir_id_clone = process_media_info.media_dir_id.clone();

  let block_thread_result = web::block(|| {
    return select_media_dir(pool_clone, media_dir_id_clone);
  }).await;

  let media_dir = match block_thread_result
  {
    Ok(select_media_dir_result) => {
      match select_media_dir_result {
        Ok(media_dir) => { media_dir },
        Err(err) => { return Err(err.into()); }
      }
    },
    Err(err) => { return Err(blocking_error(err).into()); }
  };

  let proc_dir_path = format!("{}/proc/extras", media_dir.real_path);

  match create_dir_all(&proc_dir_path) {
    Ok(_) => {},
    Err(err) => {
      let err_msg = format!("{:?}: {:?}\nError: {:?}",
        MKErrorType::CreateDirAllError.to_string(), proc_dir_path, err);

      eprintln!("{err_msg:?}");
      return Err(MKError::new(MKErrorType::CreateDirAllError, err_msg).into());
    }
  }

  tokio::spawn(async move {

    let batch_id_c_str =
      match CString::new(batch.id.clone()) {
        Ok(batch_id_c_str) => { batch_id_c_str },
        Err(err) => {
          let err_msg = format!("{:?}\nError: {:?}",
            MKErrorType::RustToCTypeConversionError.to_string(), err);

          eprintln!("{err_msg}");
          return Err(MKError::new(MKErrorType::RustToCTypeConversionError,
            err_msg).into());
        }
      };

    unsafe {
      let block_thread_result = web::block(move || {
        let batch_id_c_char: *const c_char = batch_id_c_str.as_ptr();
        return av::process_media(batch_id_c_char);
      }).await;

      match block_thread_result {
        Ok(process_media_ret) =>
        {
          if process_media_ret < 0 {
            println!("Failed to process media for batch id {:?}.",
            batch.id);
          }
        },
        Err(err) => { return Err(blocking_error(err)); }
      };
    }

    return Ok(());
  });

  return Ok("hi".to_string());
}

#[post("/abort_batch")]
pub async fn abort_batch(abort_batch_info: web::Json<AbortBatchInfo>,
  pool: web::Data<DBPool>) -> Result<String, MKError>
{
  let pool_clone = pool.clone();
  let batch_id_clone = abort_batch_info.batch_id.clone();

  let block_thread_result = web::block(move || {
    return update_batch_abort(batch_id_clone, pool_clone);
  }).await;

    match block_thread_result
    {
      Ok(update_batch_abort_result) => {
        match update_batch_abort_result {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }
      },
      Err(err) => { return Err(blocking_error(err)); }
    };

  return Ok(format!("Signaling to abort batch {:?}",
    &abort_batch_info.batch_id));
}
