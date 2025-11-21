use crate::db::config::{
  db_connect::{get_db_conn, DBPool},
  models::{
    Library,
    Collection,
    NewCollection,
    UpdateCollection,
    CollectionMovie,
    NewCollectionMovie,
    CollectionMovieInfo,
    DeleteCollectionMovie,
    CollectionShow,
    NewCollectionShow,
    CollectionShowInfo,
    DeleteCollectionShow,
    Show,
    MediaDir,
  },
  schema::{
    collection_movies, collection_shows, collections, media_dirs, shows
  }
};
use crate::utils::mk_error::{MKError, MKErrorType};

use actix_web::web;
use diesel::prelude::*;
use uuid::Uuid;

pub fn create_collection(pool: web::Data<DBPool>, new_collection: NewCollection)
  -> Result<Collection, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let collection = Collection {
    id: Uuid::new_v4().to_string(),
    name: new_collection.name.clone(),
    ino: new_collection.ino,
    device_id: new_collection.device_id,
    library_id: new_collection.library_id
  };

  let collection_result = diesel::insert_into(collections::table)
    .values(&collection).get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to create new collection: {:?}\nError: {:?}",
        new_collection.name, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return collection_result;
}

pub fn select_collections(pool: web::Data<DBPool>, library: Library)
-> Result<Vec<Collection>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let collections_result = collections::table
    .filter(collections::library_id.eq(&library.id))
    .get_results::<Collection>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get collections for library:
      {:?}, {:?}\nError: {:?}", library.name, library.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return  collections_result;
}

pub fn update_collection(pool: web::Data<DBPool>,
  update_collection: UpdateCollection) -> Result<Collection, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let updated_collection_result = diesel::update(collections::table
    .filter(collections::id.eq(&update_collection.id)))
    .set(collections::name.eq(&update_collection.name))
    .get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to update collection: {:?}, {:?}\nError: {:?}",
        update_collection.name, update_collection.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return updated_collection_result;
}

pub fn delete_collection(pool: web::Data<DBPool>, collection: Collection)
  -> Result<Collection, MKError>
{
  let pool_clone = pool.clone();
  let collection_clone = collection.clone();

  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let delete_collection_result = db.transaction(|db|
  {
    match delete_collection_movies_for_collection(pool_clone.clone(),
      collection_clone.clone())
    {
      Ok(_) => {},
      Err(err) => { return Err(err); }
    }

    match delete_collection_shows_for_collection(pool_clone, collection_clone)
    {
      Ok(_) => {},
      Err(err) => { return Err(err); }
    }

    return diesel::delete(collections::table)
      .filter(collections::id.eq(&collection.id))
      .get_result::<Collection>(db)
      .map_err(|err| {
        let err_ctx_msg = format!("Failed to delete collection:
        {:?}, {:?}\nError: {:?}", collection.name, collection.id, err);

        eprintln!("{err_ctx_msg:?}");
        return MKError::new(MKErrorType::DBError, err_ctx_msg);
      });
  });

  return delete_collection_result;
}

pub fn create_collection_show(pool: web::Data<DBPool>,
  new_collection_show: NewCollectionShow) -> Result<CollectionShow, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let collection_show = CollectionShow {
    id: Uuid::new_v4().to_string(),
    show_id: new_collection_show.show_id.clone(),
    collection_id: new_collection_show.collection_id.clone()
  };

  let collection_show_result = diesel::insert_into(collection_shows::table)
    .values(&collection_show).get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to add show:
        {:?} to collection: {:?}, {:?}\nError: {:?}",
        new_collection_show.show_path, new_collection_show.collection_name,
        new_collection_show.collection_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return collection_show_result;
}

pub fn select_collection_shows(pool: web::Data<DBPool>, collection: Collection)
  -> Result<Vec<CollectionShowInfo>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let collection_shows_result = collection_shows::table
    .inner_join(shows::table.on(collection_shows::show_id.eq(shows::id)))
    .inner_join(collections::table.on(collection_shows::collection_id.eq(collections::id)))
    .filter(collection_shows::collection_id.eq(&collection.id))
    .select((
      collection_shows::id,
      collections::id,
      collections::name,
      shows::id,
      shows::ino,
      shows::device_id,
      // shows::name,
      shows::real_path,
      // shows::symlink_path,
      // shows::static_path,
      // shows::library_id,
      // shows::library_dir_id
    ))
    .get_results::<CollectionShowInfo>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get shows for collection:
        {:?}, {:?}\nError: {:?}", collection.name, collection.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return collection_shows_result;
}

pub fn delete_collection_show(pool: web::Data<DBPool>,
  collection_show: DeleteCollectionShow) -> Result<Option<CollectionShow>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_collection_show = diesel::delete(collection_shows::table)
    .filter(collection_shows::id.eq(&collection_show.id))
    .get_result::<CollectionShow>(&mut db).optional()
    .map_err(|err| {
      let err_msg = format!("Failed to remove show:
      {:?}, {:?} from collection: {:?}, {:?}\nError: {:?}",
      collection_show.show_path, collection_show.show_id,
      collection_show.collection_name, collection_show.collection_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_collection_show;
}

pub fn delete_collection_shows_for_collection(pool: web::Data<DBPool>,
  collection: Collection) -> Result<Vec<CollectionShow>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_collection_shows = diesel::delete(collection_shows::table)
    .filter(collection_shows::collection_id.eq(&collection.id))
    .get_results::<CollectionShow>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to remove shows for collection:
      {:?}, {:?}\nError: {:?}",collection.name, collection.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_collection_shows;
}

pub fn delete_collection_shows_for_show(pool: web::Data<DBPool>,
  show: Show) -> Result<Vec<CollectionShow>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_collection_shows = diesel::delete(collection_shows::table)
    .filter(collection_shows::show_id.eq(&show.id))
    .get_results::<CollectionShow>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to delete collection shows for show:
        {:?}, {:?}\nError: {:?}", show.name, show.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_collection_shows;
}

pub fn create_collection_movie(pool: web::Data<DBPool>,
  new_collection_movie: NewCollectionMovie)
  -> Result<CollectionMovie, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let collection_movie = CollectionMovie {
    id: Uuid::new_v4().to_string(),
    movie_id: new_collection_movie.movie_id.clone(),
    collection_id: new_collection_movie.collection_id.clone()
  };

  let collection_movie_result = diesel::insert_into(collection_movies::table)
    .values(&collection_movie).get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to add movie:
        {:?} to collection: {:?}, {:?}\nError: {:?}",
        new_collection_movie.movie_path, new_collection_movie.collection_name,
        new_collection_movie.collection_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return collection_movie_result;
}

pub fn select_collection_movies(pool: web::Data<DBPool>, collection: Collection)
-> Result<Vec<CollectionMovieInfo>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let collection_movies_result = collection_movies::table
    .inner_join(media_dirs::table
      .on(collection_movies::movie_id.eq(media_dirs::id)))
    .inner_join(collections::table
      .on(collection_movies::collection_id.eq(collections::id)))
    .filter(collection_movies::collection_id.eq(&collection.id))
    .select((
      collection_movies::id,
      collections::id,
      collections::name,
      // collection_movies::collection_id,
      media_dirs::id,
      media_dirs::ino,
      media_dirs::device_id,
      // media_dirs::name,
      media_dirs::real_path,
      // media_dirs::symlink_path,
      // media_dirs::static_path,
      // media_dirs::thumbnail_url,
      // media_dirs::library_id,
      // media_dirs::library_dir_id,
      // media_dirs::show_id
    ))
    .get_results::<CollectionMovieInfo>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get movies for collection:
        {:?}, {:?}\nError: {:?}", collection.name, collection.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return collection_movies_result;
}

pub fn delete_collection_movie(pool: web::Data<DBPool>,
  collection_movie: DeleteCollectionMovie) -> Result<Option<CollectionMovie>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_collection_movie = diesel::delete(collection_movies::table)
    .filter(collection_movies::id.eq(&collection_movie.id))
    .get_result::<CollectionMovie>(&mut db).optional()
    .map_err(|err| {
      let err_msg = format!("Failed to remove movie:
        {:?}, {:?} from collection: {:?}, {:?}\nError: {:?}",
        collection_movie.movie_path, collection_movie.movie_id,
        collection_movie.collection_name, collection_movie.collection_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_collection_movie;
}

pub fn delete_collection_movies_for_collection(pool: web::Data<DBPool>,
  collection: Collection) -> Result<Vec<CollectionMovie>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_collection_movies = diesel::delete(collection_movies::table)
    .filter(collection_movies::collection_id.eq(&collection.id))
    .get_results::<CollectionMovie>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to delete collection movies for collection:
        {:?}, {:?}\nError: {:?}", collection.name, collection.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_collection_movies;
}

pub fn delete_collection_movies_for_movie(pool: web::Data<DBPool>,
  movie: MediaDir) -> Result<Vec<CollectionMovie>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let deleted_collection_movies = diesel::delete(collection_movies::table)
    .filter(collection_movies::movie_id.eq(&movie.id))
    .get_results::<CollectionMovie>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to remove movie:
        {:?}, {:?} from collections\nError: {:?}",
        movie.name, movie.id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return deleted_collection_movies;
}
