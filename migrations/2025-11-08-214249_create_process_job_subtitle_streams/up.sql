CREATE TABLE IF NOT EXISTS process_job_subtitle_streams (
  id VARCHAR PRIMARY KEY NOT NULL,
  title VARCHAR,
  burn_in BOOL NOT NULL,
  stream_id VARCHAR REFERENCES video_streams(id) NOT NULL,
  process_job_id VARCHAR REFERENCES process_jobs(id) NOT NULL
);
