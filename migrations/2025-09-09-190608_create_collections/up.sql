CREATE TABLE IF NOT EXISTS collections (
  id VARCHAR PRIMARY KEY NOT NULL,
  name VARCHAR NOT NULL,
  ino VARCHAR,
  device_id VARCHAR,
  library_id VARCHAR REFERENCES libraries(id)
);
