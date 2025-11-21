CREATE TABLE IF NOT EXISTS videos (
  id VARCHAR PRIMARY KEY NOT NULL,
  name VARCHAR NOT NULL,
  title VARCHAR,
  suggested_title VARCHAR,
  real_path VARCHAR NOT NULL,
  static_path VARCHAR NOT NULL,
  bitrate INT,
  extra BOOL NOT NULL,
  processed BOOL NOT NULL,
  thumbnail_url VARCHAR,
  media_dir_id VARCHAR REFERENCES media_dirs(id) NOT NULL
);
