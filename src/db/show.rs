use crate::db::{
    config::{
    db_connect::{get_db_conn, DBPool},
    models::{Library, Show, NewShow, UpdateShow},
    schema::shows
  },
  collection::delete_collection_shows_for_show,
  media::delete_seasons_for_show
};

use crate::utils::mk_error::{MKError, MKErrorType};

use actix_web::web;
use diesel::prelude::*;
use uuid::Uuid;

pub fn create_show(pool: web::Data<DBPool>, new_show: NewShow)
  -> Result<Show, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let show = Show {
    id: Uuid::new_v4().to_string(),
    ino: new_show.ino,
    device_id: new_show.device_id,
    name: new_show.name,
    real_path: new_show.real_path.clone(),
    symlink_path: new_show.symlink_path,
    static_path: new_show.static_path,
    library_id: new_show.library_id,
    library_dir_id: new_show.library_dir_id
  };

  let show_result = diesel::insert_into(shows::table)
    .values(&show).get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to create show: {:?}\nError: {:?}",
        new_show.real_path, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });
  
  return show_result;
}

pub fn select_shows(pool: web::Data<DBPool>, library: Library)
  -> Result<Vec<Show>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let shows_result = shows::table
    .filter(shows::library_id.eq(&library.id))
    .get_results::<Show>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get shows for library:
        {:?}, {:?}\nError: {:?}", library.name, library.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

    return shows_result;
}

pub fn update_show(pool: web::Data<DBPool>, update_show: UpdateShow)
  -> Result<Show, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let updated_show_result = diesel::update(shows::table
  .filter(shows::id.eq(&update_show.id)))
  .set((
    shows::name.eq(&update_show.name),
    shows::real_path.eq(&update_show.real_path),
    shows::symlink_path.eq(&update_show.symlink_path),
    shows::static_path.eq(&update_show.static_path)
  ))
  .get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to update show:
        {:?}, {:?}\nError: {:?}", update_show.name, update_show.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return updated_show_result;
}

pub fn delete_show(pool: web::Data<DBPool>, show: Show)
  -> Result<Option<Show>, MKError>
{
  let pool_clone = pool.clone();
  let show_clone = show.clone();

  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let delete_show_result = db.transaction(|db|
  {
    match delete_collection_shows_for_show(pool_clone.clone(),
    show_clone.clone())
    {
      Ok(_) => {},
      Err(err) => { return Err(err); }
    }

    match delete_seasons_for_show(pool_clone, show_clone) {
      Ok(_) => {},
      Err(err) => { return Err(err); }
    }

    let deleted_show_result = diesel::delete(shows::table)
      .filter(shows::id.eq(&show.id))
      .get_result::<Show>(db).optional()
      .map_err(|err| {
        let err_msg = format!("Failed to delete show: {:?}, {:?}\nError: {:?}",
          show.name, show.id, err);

        eprintln!("{err_msg:?}");
        return MKError::new(MKErrorType::DBError, err_msg);
      });

    return deleted_show_result;
  });

  return delete_show_result;
}
