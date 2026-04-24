use crate::db::{
  config::{
    db_connect::{ DBPool, get_db_conn },
    models::{
      Stream,
      VideoStreams,
      MediaDirStreams,
      Batch,
      ProcessJob,
      ProcessJobVideoStream,
      ProcessJobAudioStream,
      ProcessJobSubtitleStream,
      ProcessJobInfo,
      ProcessJobStreams,
    },
    schema::{
      media_dirs,
      videos,
      streams,
      batches,
      process_jobs,
      process_job_video_streams,
      process_job_audio_streams,
      process_job_subtitle_streams,
    }
  },
  media::{ select_video, select_videos_by_id }
};

use crate::utils::{
  mk_error::{ MKError, MKErrorType, blocking_error },
  proc::{ ProcessMediaParams, ProcessVideoParams, ProcessJobStatus }
};

use actix_web::web;
use chrono::NaiveDateTime;
use diesel::prelude::*;
use uuid::Uuid;

pub async fn get_media_dir_streams(media_dir_id: String,
  pool: web::Data<DBPool>) -> Result<MediaDirStreams, MKError>
{
  let pool_clone = pool.clone();
  let media_dir_id_clone = media_dir_id.clone();

  let videos = match select_videos_by_id(pool_clone, media_dir_id_clone) {
    Ok(videos) => { videos },
    Err(err) => { return Err(err); }
  };

  let mut db = match get_db_conn(pool.clone()) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let mut media_dirs_streams: MediaDirStreams = vec![] ;

  for video in videos.clone() {
    if video.extra { continue; }

    let streams_result = streams::table
      .filter(streams::video_id.eq(&video.id))
      .get_results::<Stream>(&mut db)
      .map_err(|err| {
        let mut err_msg = format!("Failed to get video with id: '{}'. ",
          video.id);
        err_msg = format!("{err_msg}Error: {}.", err);
        eprintln!("{err_msg:?}");
      });

    let streams = match streams_result {
      Ok(streams) => { streams },
      Err(_err) => { continue; }
    };

    let video_streams = VideoStreams {
      video: video,
      streams: streams
    };

    media_dirs_streams.push(video_streams);
  }

  for video in videos {
    if !video.extra { continue; }

    let streams_result = streams::table
      .filter(streams::video_id.eq(&video.id))
      .get_results::<Stream>(&mut db)
      .map_err(|err| {
        let mut err_msg = format!("Failed to get video with id: '{}'. ",
          video.id);
        err_msg = format!("{err_msg}Error: {}.", err);
        eprintln!("{err_msg:?}");
      });

    let streams = match streams_result {
      Ok(streams) => { streams },
      Err(_err) => { continue; }
    };

    let video_streams = VideoStreams {
      video: video,
      streams: streams
    };

    media_dirs_streams.push(video_streams);
  }

  return Ok(media_dirs_streams);
}

pub async fn create_batch(process_media_info: ProcessMediaParams,
  pool: web::Data<DBPool>) -> Result<Batch, MKError>
{
  let mut db = match get_db_conn(pool.clone()) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let batch_id = Uuid::new_v4().to_string();

  let batch = Batch {
    id: batch_id.clone(),
    batch_size: process_media_info.videos.len() as i32,
    aborted: false
  };

  let create_batch_result = diesel::insert_into(batches::table)
    .values(&batch)
    .get_result::<Batch>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to create new batch: \nError: {:?}", err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError,
        err_msg);
    });

  let mut pool_clone: web::Data<DBPool>;
  let mut batch_id_clone: String;
  let mut video_info_clone: ProcessVideoParams;
  let created = chrono::Utc::now().naive_utc();

  for video_info in process_media_info.videos.clone()
  {
    pool_clone = pool.clone();
    batch_id_clone = batch_id.clone();
    video_info_clone = video_info.clone();

    let block_thread_result = web::block(move || {
      return create_process_job(batch_id_clone,
        video_info_clone, created, pool_clone);
    }).await;

    match block_thread_result {
      Ok(create_process_job_result) => {
        match create_process_job_result.await
        {
          Ok(_) => {},
          Err(err) => { return Err(err); }
        }
      },
      Err(err) => { return Err(blocking_error(err)); }
    }
  }

  return create_batch_result;
}

pub async fn create_process_job(batch_id: String,
  video_info: ProcessVideoParams, created: NaiveDateTime,
  pool: web::Data<DBPool>) -> Result<ProcessJob, MKError>
{
  let mut stream_count = 1;
  let pool_clone = pool.clone();
  let video_id_clone = video_info.video_id.clone();

  let block_thread_result = web::block(|| {
    return select_video(pool_clone, video_id_clone);
  }).await;

  let video = match block_thread_result
  {
    Ok(select_video_result) => {
      match select_video_result {
        Ok(video) => { video },
        Err(err) => { return Err(err.into()); }
      }
    },
    Err(err) => { return Err(blocking_error(err)); }
  };

  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let create_process_job_result = db.transaction(|db|
  {
    let process_job = ProcessJob {
      id: Uuid::new_v4().to_string(),
      title: video_info.title,
      job_status: ProcessJobStatus::Pending.to_string(),
      stream_count: None,
      pct_complete: 0,
      err_msg: None,
      created: created,
      video_id: video_info.video_id.clone(),
      batch_id: batch_id,
    };

    let process_job_result =
      diesel::insert_into(process_jobs::table)
        .values(&process_job)
        .get_result(db)
        .map_err(|err| {
          let err_msg = format!("Failed to create new process job for video:
          {:?}, {:?}\nError: {:?}", video.name, video.id, err);

          eprintln!("{err_msg:?}");
          return MKError::new(MKErrorType::DBError,
            err_msg);
        });

    let process_job_video_stream = ProcessJobVideoStream {
      id: Uuid::new_v4().to_string(),
      title: video_info.video_stream.title,
      passthrough: video_info.video_stream.passthrough,
      codec: video_info.video_stream.codec,
      hwaccel: video_info.video_stream.hwaccel,
      deinterlace: video_info.video_stream.deinterlace,
      create_renditions: video_info.video_stream.create_renditions,
      title2: video_info.video_stream.title2,
      codec2: video_info.video_stream.codec2,
      hwaccel2: video_info.video_stream.hwaccel2,
      tonemap: video_info.video_stream.tonemap,
      stream_id: video_info.video_stream.id,
      process_job_id: process_job.id.clone()
    };

    let _ = diesel::insert_into(process_job_video_streams::table)
      .values(&process_job_video_stream)
      .get_result::<ProcessJobVideoStream>(db)
      .map_err(|err| {
        let err_msg = format!("Failed to create new process job video stream
        for video: {:?}, {:?}\nError: {:?}", video.name, video.id, err);

        eprintln!("{err_msg:?}");
        return MKError::new(MKErrorType::DBError,
          err_msg);
      });

    let mut process_job_audio_stream: ProcessJobAudioStream;

    for audio_stream in video_info.audio_streams
    {
      stream_count += 1;
      process_job_audio_stream = ProcessJobAudioStream {
        id: Uuid::new_v4().to_string(),
        title: audio_stream.title,
        passthrough: audio_stream.passthrough,
        gain_boost: audio_stream.gain_boost,
        create_renditions: audio_stream.create_renditions,
        title2: audio_stream.title2,
        gain_boost2: audio_stream.gain_boost2,
        stream_id: audio_stream.id,
        process_job_id: process_job.id.clone()
      };

      let _ = diesel::insert_into(process_job_audio_streams::table)
        .values(&process_job_audio_stream)
        .get_result::<ProcessJobAudioStream>(db)
        .map_err(|err| {
          let err_msg = format!("Failed to create new process job audio stream
          for video: {:?}, {:?}\nError: {:?}", video.name, video.id, err);

          eprintln!("{err_msg:?}");
          return MKError::new(MKErrorType::DBError,
            err_msg);
        });
    }

    let mut process_job_subtitle_stream: ProcessJobSubtitleStream;

    for subtitle_stream in video_info.subtitle_streams
    {
      stream_count += 1;
      process_job_subtitle_stream = ProcessJobSubtitleStream {
        id: Uuid::new_v4().to_string(),
        title: subtitle_stream.title,
        burn_in: subtitle_stream.burn_in,
        stream_id: subtitle_stream.id,
        process_job_id: process_job.id.clone()
      };

      let _ = diesel::insert_into(process_job_subtitle_streams::table)
        .values(&process_job_subtitle_stream)
        .get_result::<ProcessJobSubtitleStream>(db)
        .map_err(|err| {
          let err_msg = format!("Failed to create new process job subtitle stream
          for video: {:?}, {:?}\nError: {:?}", video.name, video.id, err);

          eprintln!("{err_msg:?}");
          return MKError::new(MKErrorType::DBError,
            err_msg);
        });
    }

    let _ = diesel::update(process_jobs::table
      .filter(process_jobs::id.eq(process_job.id)))
      .set(process_jobs::stream_count.eq(&stream_count))
      .get_result::<ProcessJob>(db)
      .map_err(|err| {
        let err_msg = format!("Failed to update process_job stream_count:
        {:?}\nError: {:?}", video_info.video_id, err);

        eprintln!("{err_msg:?}");
        return MKError::new(MKErrorType::DBError, err_msg);
      });

    return process_job_result;
  });

  return create_process_job_result;
}

pub fn update_batch_abort(batch_id: String, pool: web::Data<DBPool>)
  -> Result<Batch, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let updated_batch_result =
    diesel::update(batches::table
    .filter(batches::id.eq(&batch_id)))
    .set(batches::aborted.eq(true))
    .get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to abort batch: {:?}
        \nError: {:?}", batch_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return updated_batch_result;
}

pub fn select_process_jobs_for_batch_id(pool: web::Data<DBPool>, batch_id: String)
  -> Result<Vec<ProcessJob>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let process_jobs_result = process_jobs::table
    .filter(process_jobs::batch_id.eq(&batch_id))
    .get_results::<ProcessJob>(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to get process jobs for batch id:
        {:?}\nError: {:?}", &batch_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return process_jobs_result;
}

pub fn select_process_jobs_for_media_dir(media_dir_id: String,
  pool: web::Data<DBPool>) -> Result<Vec<ProcessJobStreams>, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let process_jobs = match process_jobs::table
    .inner_join(videos::table.on(process_jobs::video_id.eq(videos::id)))
    .inner_join(media_dirs::table.on(videos::media_dir_id.eq(media_dirs::id)))
    .filter(media_dirs::id.eq(&media_dir_id))
    .order_by(process_jobs::created.desc())
    .select((
      process_jobs::id,
      videos::name,
      process_jobs::created,
      process_jobs::job_status,
      process_jobs::pct_complete,
    ))
    .get_results::<ProcessJobInfo>(&mut db)
    {
      Ok(process_jobs) => { process_jobs },
      Err(err) => {
        let err_msg = format!("Failed to get process_jobs for media_dir:
          {:?}\nError: {:?}", media_dir_id, err);

        eprintln!("{err_msg:?}");
        return Err(MKError::new(MKErrorType::DBError, err_msg));
      }
    };

    let mut process_jobs_streams_vec: Vec<ProcessJobStreams> = vec![];

    for process_job in process_jobs {
      let process_jobs_video_stream = match process_job_video_streams::table
        .filter(process_job_video_streams::process_job_id
          .eq(&process_job.process_job_id))
        .get_result::<ProcessJobVideoStream>(&mut db)
        {
          Ok(process_jobs_video_stream) => { process_jobs_video_stream },
          Err(err) => {
            let err_msg = format!("Failed to get process_job_video_stream \
for process_job: {:?}\nError: {:?}", process_job.process_job_id, err);

            eprintln!("{err_msg:?}");
            return Err(MKError::new(MKErrorType::DBError, err_msg));
          }
        };

      let process_job_audio_stream_vec = match process_job_audio_streams::table
        .filter(process_job_audio_streams::process_job_id
          .eq(&process_job.process_job_id))
        .get_results::<ProcessJobAudioStream>(&mut db)
        {
          Ok(process_jobs_audio_stream_vec) => { process_jobs_audio_stream_vec },
          Err(err) => {
            let err_msg = format!("Failed to get process_job_audio_streams \
for process_job: {:?}\nError: {:?}", process_job.process_job_id, err);

            eprintln!("{err_msg:?}");
            return Err(MKError::new(MKErrorType::DBError, err_msg));
          }
        };

      let process_job_subtitle_stream_vec = match process_job_subtitle_streams::table
        .filter(process_job_subtitle_streams::process_job_id
          .eq(&process_job.process_job_id))
        .get_results::<ProcessJobSubtitleStream>(&mut db)
        {
          Ok(process_jobs_subtitle_stream_vec) => { process_jobs_subtitle_stream_vec },
          Err(err) => {
            let err_msg = format!("Failed to get process_job_subtitle_streams \
for process_job: {:?}\nError: {:?}", process_job.process_job_id, err);

            eprintln!("{err_msg:?}");
            return Err(MKError::new(MKErrorType::DBError, err_msg));
          }
        };

      let process_job_streams = ProcessJobStreams {
        process_job: process_job,
        video_stream: process_jobs_video_stream,
        audio_streams: process_job_audio_stream_vec,
        subtitle_streams: process_job_subtitle_stream_vec
      };

      process_jobs_streams_vec.push(process_job_streams);
    }

  return Ok(process_jobs_streams_vec);
}

pub fn update_status_process_job(pool: web::Data<DBPool>,
  process_job: ProcessJob, job_status: ProcessJobStatus)
  -> Result<ProcessJob, MKError>
{
  let mut db = match get_db_conn(pool) {
    Ok(db) => { db }, Err(err) => { return Err(err); }
  };

  let updated_process_job =
    diesel::update(process_jobs::table
    .filter(process_jobs::id.eq(&process_job.id)))
    .set(process_jobs::job_status.eq(job_status.to_string()))
    .get_result(&mut db)
    .map_err(|err| {
      let err_msg = format!("Failed to update process job: {:?} for video: {:?}
        \nError: {:?}", process_job.id, process_job.video_id, err);

      eprintln!("{err_msg:?}");
      return MKError::new(MKErrorType::DBError, err_msg);
    });

  return updated_process_job;
}
