// use crate::db::config::models::{
//   MediaDir,
//   Video
// };

use derive_more::Display;
use serde::Deserialize;

#[derive(Deserialize, Clone)]
pub struct ProcessVideoStreamInfo {
  pub id: String,
  pub title: Option<String>,
  pub passthrough: bool,
  pub create_renditions: bool,
  pub title2: Option<String>,
  pub tonemap: bool,
  pub deinterlace: bool
}

#[derive(Deserialize, Clone)]
pub struct ProcessAudioStreamInfo {
  pub id: String,
  pub title: Option<String>,
  pub passthrough: bool,
  pub create_renditions: bool,
  pub gain_boost: i32
}

#[derive(Deserialize, Clone)]
pub struct ProcessSubtitleStreamInfo {
  pub id: String,
  pub title: Option<String>,
  pub burn_in: bool
}

#[derive(Deserialize, Clone)]
pub struct ProcessVideoInfo {
  pub video_id: String,
  pub title: Option<String>,
  pub video_stream: ProcessVideoStreamInfo,
  pub audio_streams: Vec<ProcessAudioStreamInfo>,
  pub subtitle_streams: Vec<ProcessSubtitleStreamInfo>,
  // pub process_id: Option<String>
}

#[derive(Deserialize, Clone)]
pub struct ProcessMediaInfo {
  pub media_dir_id: String,
  pub videos: Vec<ProcessVideoInfo>,
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
