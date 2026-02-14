CREATE TABLE IF NOT EXISTS process_job_video_streams (
  id VARCHAR PRIMARY KEY NOT NULL,
  title VARCHAR,
  passthrough BOOL NOT NULL,
  codec INT NOT NULL,
  hwaccel BOOL NOT NULL,
  deinterlace BOOL NOT NULL,
  create_renditions BOOL NOT NULL,
  title2 VARCHAR,
  codec2 INT NOT NULL,
  hwaccel2 BOOL NOT NULL,
  tonemap BOOL NOT NULL,
  stream_id VARCHAR REFERENCES video_streams(id) NOT NULL,
  process_job_id VARCHAR REFERENCES process_jobs(id) NOT NULL
);
