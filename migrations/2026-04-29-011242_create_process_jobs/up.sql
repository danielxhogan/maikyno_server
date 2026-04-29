CREATE TABLE IF NOT EXISTS process_jobs (
  id VARCHAR PRIMARY KEY NOT NULL,
  title VARCHAR,
  job_status VARCHAR NOT NULL,
  stream_count INT,
  pct_complete INT NOT NULL,
  err_msg VARCHAR,
  video_id VARCHAR REFERENCES videos(id) NOT NULL,
  batch_id VARCHAR REFERENCES batches(id) NOT NULL
);
