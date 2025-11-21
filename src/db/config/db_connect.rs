use crate::utils::mk_error::{MKError, MKErrorType};

use diesel::{r2d2, SqliteConnection};
use actix_web::web;
use dotenvy::dotenv;

pub type DBPool = r2d2::Pool<r2d2::ConnectionManager<SqliteConnection>>;
pub type DBConn = r2d2::PooledConnection<r2d2::ConnectionManager<SqliteConnection>>;

pub fn db_pool_init() -> DBPool
{
  dotenv().ok();

  let database_url = std::env::var("DATABASE_URL")
    .expect("Failed to get 'DATABASE_URL' env var.");

  let con_man = r2d2::ConnectionManager::<SqliteConnection>::new(database_url);

  let pool = r2d2::Pool::builder().build(con_man)
    .expect("Failed to create database connection pool.");

  return pool;
}

pub fn get_db_conn(pool: web::Data<DBPool>) -> Result<DBConn, MKError>
{
  let get_ret = pool.get();
  let db = match get_ret
  {
    Ok(db) => { db },
    Err(err) => {
      let err_msg = format!("{:?}.\nError: {:?}",
        MKErrorType::R2D2Error.to_string(), err);

      eprintln!("{err_msg:?}");
      return Err(MKError::new(MKErrorType::R2D2Error, err_msg));
    }
  };

  return Ok(db);
}
