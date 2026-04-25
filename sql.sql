select * from libraries;

select id, name from media_dirs where library_id = "";

 select id, name, og, processed, static_path from videos where media_dir_id = "";

select
    movies.name, movie_versions.name, movie_versions.processed, collections.name, movies.path
    from collection_movies join movies on collection_movies.movie_id = movies.id
    join collections on collection_movies.collection_id = collections.id
    join movie_versions on collection_movies.movie_id = movie_versions.movie_id;
