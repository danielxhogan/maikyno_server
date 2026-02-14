// @generated automatically by Diesel CLI.

diesel::table! {
    batches (id) {
        id -> Text,
        batch_size -> Integer,
        aborted -> Bool,
    }
}

diesel::table! {
    collection_movies (id) {
        id -> Text,
        movie_id -> Text,
        collection_id -> Text,
    }
}

diesel::table! {
    collection_shows (id) {
        id -> Text,
        show_id -> Text,
        collection_id -> Text,
    }
}

diesel::table! {
    collections (id) {
        id -> Text,
        name -> Text,
        ino -> Nullable<Text>,
        device_id -> Nullable<Text>,
        library_id -> Nullable<Text>,
    }
}

diesel::table! {
    libraries (id) {
        id -> Text,
        name -> Text,
        media_type -> Text,
    }
}

diesel::table! {
    library_dirs (id) {
        id -> Text,
        ino -> Text,
        device_id -> Text,
        name -> Text,
        real_path -> Text,
        symlink_path -> Text,
        static_path -> Text,
        library_id -> Text,
    }
}

diesel::table! {
    media_dirs (id) {
        id -> Text,
        ino -> Text,
        device_id -> Text,
        name -> Text,
        real_path -> Text,
        symlink_path -> Text,
        static_path -> Text,
        thumbnail_url -> Nullable<Text>,
        library_id -> Text,
        library_dir_id -> Text,
        show_id -> Nullable<Text>,
    }
}

diesel::table! {
    process_job_audio_streams (id) {
        id -> Text,
        title -> Nullable<Text>,
        passthrough -> Bool,
        gain_boost -> Integer,
        create_renditions -> Bool,
        title2 -> Nullable<Text>,
        gain_boost2 -> Integer,
        stream_id -> Text,
        process_job_id -> Text,
    }
}

diesel::table! {
    process_job_subtitle_streams (id) {
        id -> Text,
        title -> Nullable<Text>,
        burn_in -> Bool,
        stream_id -> Text,
        process_job_id -> Text,
    }
}

diesel::table! {
    process_job_video_streams (id) {
        id -> Text,
        title -> Nullable<Text>,
        passthrough -> Bool,
        codec -> Integer,
        hwaccel -> Bool,
        deinterlace -> Bool,
        create_renditions -> Bool,
        title2 -> Nullable<Text>,
        codec2 -> Integer,
        hwaccel2 -> Bool,
        tonemap -> Bool,
        stream_id -> Text,
        process_job_id -> Text,
    }
}

diesel::table! {
    process_jobs (id) {
        id -> Text,
        title -> Nullable<Text>,
        job_status -> Text,
        stream_count -> Nullable<Integer>,
        pct_complete -> Integer,
        err_msg -> Nullable<Text>,
        video_id -> Text,
        batch_id -> Text,
    }
}

diesel::table! {
    shows (id) {
        id -> Text,
        ino -> Text,
        device_id -> Text,
        name -> Text,
        real_path -> Text,
        symlink_path -> Text,
        static_path -> Text,
        library_id -> Text,
        library_dir_id -> Text,
    }
}

diesel::table! {
    streams (id) {
        id -> Text,
        stream_idx -> Integer,
        title -> Nullable<Text>,
        stream_type -> Integer,
        codec -> Text,
        height -> Nullable<Integer>,
        width -> Nullable<Integer>,
        interlaced -> Nullable<Bool>,
        video_id -> Text,
    }
}

diesel::table! {
    videos (id) {
        id -> Text,
        name -> Text,
        title -> Nullable<Text>,
        suggested_title -> Nullable<Text>,
        real_path -> Text,
        static_path -> Text,
        bitrate -> Nullable<Integer>,
        extra -> Bool,
        processed -> Bool,
        thumbnail_url -> Nullable<Text>,
        media_dir_id -> Text,
    }
}

diesel::joinable!(collection_movies -> collections (collection_id));
diesel::joinable!(collection_movies -> media_dirs (movie_id));
diesel::joinable!(collection_shows -> collections (collection_id));
diesel::joinable!(collection_shows -> shows (show_id));
diesel::joinable!(collections -> libraries (library_id));
diesel::joinable!(library_dirs -> libraries (library_id));
diesel::joinable!(media_dirs -> libraries (library_id));
diesel::joinable!(media_dirs -> library_dirs (library_dir_id));
diesel::joinable!(media_dirs -> shows (show_id));
diesel::joinable!(process_job_audio_streams -> process_jobs (process_job_id));
diesel::joinable!(process_job_subtitle_streams -> process_jobs (process_job_id));
diesel::joinable!(process_job_video_streams -> process_jobs (process_job_id));
diesel::joinable!(process_jobs -> batches (batch_id));
diesel::joinable!(process_jobs -> videos (video_id));
diesel::joinable!(shows -> libraries (library_id));
diesel::joinable!(shows -> library_dirs (library_dir_id));
diesel::joinable!(streams -> videos (video_id));
diesel::joinable!(videos -> media_dirs (media_dir_id));

diesel::allow_tables_to_appear_in_same_query!(
    batches,
    collection_movies,
    collection_shows,
    collections,
    libraries,
    library_dirs,
    media_dirs,
    process_job_audio_streams,
    process_job_subtitle_streams,
    process_job_video_streams,
    process_jobs,
    shows,
    streams,
    videos,
);
