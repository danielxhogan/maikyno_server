CREATE TABLE IF NOT EXISTS streams (
  id VARCHAR PRIMARY KEY NOT NULL,
  stream_idx INT NOT NULL,
  title VARCHAR,
  stream_type INT NOT NULL,
  codec VARCHAR NOT NULL,
  height INT,
  width INT,
  interlaced BOOL,
  nb_channels INT,
  video_id VARCHAR NOT NULL REFERENCES videos(id)
);
