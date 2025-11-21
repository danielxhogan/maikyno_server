CREATE TABLE IF NOT EXISTS library_dirs (
  id VARCHAR PRIMARY KEY NOT NULL,
  ino VARCHAR NOT NULL,
  device_id VARCHAR NOT NULL,
  name VARCHAR NOT NULL,
  real_path VARCHAR NOT NULL,
  symlink_path VARCHAR NOT NULL,
  static_path VARCHAR NOT NULL,
  library_id VARCHAR REFERENCES libraries(id) NOT NULL
);
