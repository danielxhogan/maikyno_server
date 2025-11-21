CREATE TABLE IF NOT EXISTS process_job_audio_streams (
  id VARCHAR PRIMARY KEY NOT NULL,
  title VARCHAR,
  passthrough BOOL NOT NULL,
  create_renditions BOOL NOT NULL,
  gain_boost INT NOT NULL,
  stream_id VARCHAR REFERENCES video_streams(id) NOT NULL,
  process_job_id VARCHAR REFERENCES process_jobs(id) NOT NULL
);
