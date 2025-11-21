CREATE TABLE IF NOT EXISTS shows (
  id VARCHAR PRIMARY KEY NOT NULL,
  ino VARCHAR NOT NULL,
  device_id VARCHAR NOT NULL,
  name VARCHAR NOT NULL,
  real_path VARCHAR NOT NULL,
  symlink_path VARCHAR NOT NULL,
  static_path VARCHAR NOT NULL,
  library_id VARCHAR NOT NULL REFERENCES libraries(id),
  library_dir_id VARCHAR NOT NULL REFERENCES library_dirs(id)
);
