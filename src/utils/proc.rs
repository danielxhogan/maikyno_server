// use crate::db::config::models::{
//   MediaDir,
//   Video
// };

use derive_more::Display;
use serde::Deserialize;

#[derive(Deserialize, Clone)]
pub struct ProcessVideoStreamParams {
  pub id: String,
  pub title: Option<String>,
  pub passthrough: bool,
  pub codec: i32,
  pub hwaccel: bool,
  pub deinterlace: bool,
  pub create_renditions: bool,
  pub title2: Option<String>,
  pub codec2: i32,
  pub hwaccel2: bool,
  pub tonemap: bool
}

#[derive(Deserialize, Clone)]
pub struct ProcessAudioStreamParams {
  pub id: String,
  pub title: Option<String>,
  pub passthrough: bool,
  pub gain_boost: i32,
  pub create_renditions: bool,
  pub title2: Option<String>,
  pub gain_boost2: i32,
}

#[derive(Deserialize, Clone)]
pub struct ProcessSubtitleStreamParams {
  pub id: String,
  pub title: Option<String>,
  pub burn_in: bool,
}

#[derive(Deserialize, Clone)]
pub struct ProcessVideoParams {
  pub video_id: String,
  pub title: Option<String>,
  pub video_stream: ProcessVideoStreamParams,
  pub audio_streams: Vec<ProcessAudioStreamParams>,
  pub subtitle_streams: Vec<ProcessSubtitleStreamParams>,
}

#[derive(Deserialize, Clone)]
pub struct ProcessMediaParams {
  pub media_dir_id: String,
  pub videos: Vec<ProcessVideoParams>,
}

#[derive(Display, Clone)]
pub enum ProcessJobStatus {
  #[display("pending")]
  Pending,

  // #[display("processing")]
  // Processing,

  // #[display("complete")]
  // Complete,

  // #[display("failed")]
  // Failed,
  
  // #[display("aborted")]
  // Aborted
}
