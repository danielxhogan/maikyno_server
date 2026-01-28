#![allow(dead_code)]
mod db;
mod routes;
mod utils;

#[allow(non_snake_case, non_camel_case_types, non_upper_case_globals, improper_ctypes, dead_code)]
mod av;

use db::config::db_connect::db_pool_init;

use routes::{
  library::new_library,
  scan_library::scan_library,
  proc::{rename_extras, scan_media_streams, process_media, abort_batch}
};

use actix_web::{web, App, HttpServer};
use actix_files as af;

use std::{fs, env, path::PathBuf};

#[derive(Debug, Clone)]
pub struct AppState {
  root_media_dir_pathbuf: PathBuf,
  root_media_dir_string: String
}

#[actix_web::main]
async fn main() -> std::io::Result<()>
{
  let pool = db_pool_init();

  let home_dir = env::home_dir().expect("Failed to find home directory.");
  let root_media_dir_pathbuf = home_dir.join("mk_media");

  let root_media_dir_string = root_media_dir_pathbuf.to_str()
    .expect("Home directory path must not contain non UTF-8 values").to_string();

  fs::create_dir_all(&root_media_dir_pathbuf)
    .expect("Failed to establish root media directory.");

  let app_state = AppState {
    root_media_dir_pathbuf: root_media_dir_pathbuf.clone(),
    root_media_dir_string: root_media_dir_string.clone()
  };

  return HttpServer::new(move || {
    return App::new()
      .app_data(web::Data::new(pool.clone()))
      .app_data(web::Data::new(app_state.clone()))
      .service(new_library)
      .service(scan_library)
      .service(rename_extras)
      .service(scan_media_streams)
      .service(process_media)
      .service(abort_batch)
      .service(af::Files::new("media",
        root_media_dir_pathbuf.clone()));
    })
    .bind(("0.0.0.0", 8080))?.run().await;
}
