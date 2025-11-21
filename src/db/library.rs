use crate::db::config::{
  db_connect::{get_db_conn, DBPool},
  models::{Library, NewLibrary, LibraryDir, NewLibraryDir},
  schema::{libraries, library_dirs}
};
use crate::utils::mk_error::{MKError, MKErrorType};

use actix_web::web;
use diesel::prelude::*;
use uuid::Uuid;
use derive_more::Display;
use serde::Deserialize;

#[derive(Display, Deserialize, PartialEq)]
pub enum MediaType {
  #[display("movie")]
  Movie,

  #[display("show")]
  Show
}

pub fn create_library(pool: web::Data<DBPool>, new_library: NewLibrary,
  initial_library_dirs: Vec<NewLibraryDir>) -> Result<Library, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let create_library_result =  db.transaction(|db| {
    let library = Library {
      id: Uuid::new_v4().to_string(),
      name: new_library.name.clone(),
      media_type: new_library.media_type
    };

    let library_result = diesel::insert_into(libraries::table)
      .values(&library).get_result::<Library>(db);

    match library_result {
      Ok(library) => {
        let mut library_dir: LibraryDir;

        for new_library_dir in initial_library_dirs {
          library_dir = LibraryDir {
            id: Uuid::new_v4().to_string(),
            name: new_library_dir.name,
            real_path: new_library_dir.real_path,
            symlink_path: new_library_dir.symlink_path,
            static_path: new_library_dir.static_path,
            ino: new_library_dir.ino,
            device_id: new_library_dir.device_id,
            library_id: library.id.clone()
          };

          let library_dir_result = diesel::insert_into(library_dirs::table)
            .values(&library_dir).get_result::<LibraryDir>(db);

            match library_dir_result {
              Ok(_) => {},
              Err(err) => { return Err(err); }
            }
        }

        return Ok(library);
      },
      Err(err) => { return Err(err); }
    }
  });

  match create_library_result {
    Ok(library) => { Ok(library) },
    Err(err) => {
      let err_msg = format!("Failed to create new library:
      {:?}\nError: {:?}", &new_library.name, err);

      eprintln!("{err_msg:?}");
      return Err(MKError::new(MKErrorType::DBError, err_msg));
    }
  }
}

pub fn select_library(pool: web::Data<DBPool>, library_id: String)
-> Result<Library, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let library_result = libraries::table
    .filter(libraries::id.eq(&library_id))
    .get_result::<Library>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get library: {:?}\nError: {:?}",
        library_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return library_result;
}

pub fn select_library_dirs(pool: web::Data<DBPool>, library: Library)
  -> Result<Vec<LibraryDir>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let library_dirs_result = library_dirs::table
    .filter(library_dirs::library_id.eq(&library.id))
    .get_results::<LibraryDir>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get library dirs for library:
      {:?}\nError: {:?}", library.name, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return library_dirs_result;
}

// pub fn delete_library(pool: web::Data<DBPool>, library: Library)
// -> Result<Library, MKError>
// {
//   let mut db = match get_db_conn(pool) {
//     Ok(db) => { db }, Err(err) => { return Err(err); }
//   };

//   let delete_library_result = diesel::delete(libraries::table)
//     .filter(libraries::id.eq(library.id))
//     // .execute(&mut db)
//     .get_result::<Library>(&mut db)
//     .map_err(|err| {
//       let err_ctx_msg = format!("Failed to delete library: {:?}\nError: {:?}",
//         library.name, err);

//       eprintln!("{err_ctx_msg:?}");
//       return MKError::new(MKErrorType::DBError, err_ctx_msg);
//     });

//   return delete_library_result;
// }
