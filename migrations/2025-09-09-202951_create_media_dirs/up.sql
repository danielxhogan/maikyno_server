CREATE TABLE IF NOT EXISTS media_dirs (
  id VARCHAR PRIMARY KEY NOT NULL,
  ino VARCHAR NOT NULL,
  device_id VARCHAR NOT NULL,
  name VARCHAR NOT NULL,
  real_path VARCHAR NOT NULL,
  symlink_path VARCHAR NOT NULL,
  static_path VARCHAR NOT NULL,
  thumbnail_url VARCHAR,
  library_id VARCHAR NOT NULL REFERENCES libraries(id),
  library_dir_id VARCHAR NOT NULL REFERENCES library_dirs(id),
  show_id VARCHAR REFERENCES shows(id)
);
