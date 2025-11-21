CREATE TABLE IF NOT EXISTS collection_movies (
  id VARCHAR PRIMARY KEY NOT NULL,
  movie_id VARCHAR REFERENCES media_dirs(id) NOT NULL,
  collection_id VARCHAR REFERENCES collections(id) NOT NULL
);
