CREATE TABLE IF NOT EXISTS collection_shows (
  id VARCHAR NOT NULL PRIMARY KEY,
  show_id VARCHAR NOT NULL REFERENCES shows(id),
  collection_id VARCHAR NOT NULL REFERENCES collections(id)
);
