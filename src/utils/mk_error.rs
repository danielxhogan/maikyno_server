use actix_web::{error, HttpResponse, body::BoxBody, http::StatusCode};
use derive_more::derive::Display;
use diesel::result::Error as DieselError;

#[derive(Debug, Display, PartialEq)]
pub enum MKErrorType
{
  #[display("No data received in request")]
  NoData,

  #[display("Invalid media type")]
  InvalidMediaType,

  #[display("Failed to execute database query")]
  DBError,


  // fs
  // ********************************************************
  #[display("Could not find library path")]
  LibraryPathNotFoundError,

  #[display("Failed to create library, already exists")]
  LibraryAlreadyExists,

  #[display("Failed to create symbolic link for library")]
  LibrarySymlinkError,

  #[display("Failed to create directory")]
  CreateDirAllError,

  #[display("Failed to remove directory")]
  RemoveDirAllError,

  #[display("Failed to get parent directory for path")]
  ParentDirError,

  #[display("Failed to get String for PathBuf")]
  PathBufStringError,

  #[display("Failed to get file name for path")]
  FileNameError,

  #[display("Failed to get metadata for dir entry")]
  DirEntryMetadataError,

  #[display("Failed to read contents of directory")]
  ReadDirError,


  // proc
  // ********************************************************
  // #[display("Failed to make clips for video")]
  // MakeClipsError,

  // #[display("Failed to test difficulty for video")]
  // TestDifficultyError,

  // #[display("Failed to find process job batch with id")]
  // ProcessJobBatchIdNotFound,


  // misc
  // ********************************************************
  #[display("Failed to serialize response")]
  SerializeError,

  #[display("r2d2 Error")]
  R2D2Error,

  #[display("Blocking error")]
  BlockingError,
}


#[derive(Debug, Display)]
#[display("Error: {message}")]
pub struct MKError {
  pub err_type: MKErrorType,
  pub message: String
}

impl MKError {
  pub fn new(err_type: MKErrorType, message: String) -> MKError
  {
    return MKError { err_type, message };
  }
}

impl error::ResponseError for MKError {
  fn error_response(&self) -> HttpResponse<BoxBody>
  {
    return HttpResponse::build(self.status_code())
      .body(self.message.clone());
  }

  fn status_code(&self) -> StatusCode
  {
    return match self.err_type {
      MKErrorType::NoData => StatusCode::BAD_REQUEST,
      MKErrorType::InvalidMediaType => StatusCode::BAD_REQUEST,

      // Library
      // ***********************************************************
      MKErrorType::LibraryPathNotFoundError => StatusCode::BAD_REQUEST,
      MKErrorType::LibraryAlreadyExists => StatusCode::BAD_REQUEST,
      MKErrorType::LibrarySymlinkError => StatusCode::BAD_REQUEST,

      // Shows
      // ***********************************************************


      MKErrorType::DBError => StatusCode::BAD_REQUEST,

      // fs
      // ********************************************************
      MKErrorType::CreateDirAllError => StatusCode::BAD_REQUEST,
      MKErrorType::RemoveDirAllError => StatusCode::BAD_REQUEST,
      MKErrorType::ParentDirError => StatusCode::BAD_REQUEST,
      MKErrorType::PathBufStringError => StatusCode::BAD_REQUEST,
      MKErrorType::FileNameError => StatusCode::BAD_REQUEST,
      MKErrorType::DirEntryMetadataError => StatusCode::BAD_REQUEST,
      MKErrorType::ReadDirError => StatusCode::BAD_REQUEST,

      // misc
      // ********************************************************
      MKErrorType::SerializeError => StatusCode::INTERNAL_SERVER_ERROR,
      MKErrorType::R2D2Error => StatusCode::INTERNAL_SERVER_ERROR,
      MKErrorType::BlockingError => StatusCode::INTERNAL_SERVER_ERROR,

      // proc
      // ********************************************************
      // MKErrorType::MakeClipsError => StatusCode::BAD_REQUEST,
      // MKErrorType::TestDifficultyError => StatusCode::BAD_REQUEST,
      // MKErrorType::ProcessJobBatchIdNotFound => StatusCode::BAD_REQUEST,
    };
  }
}

pub fn blocking_error(err: error::BlockingError) -> MKError
{
  let err_msg = format!("{:?}.\nError: {:?}.",
    MKErrorType::BlockingError.to_string(), err);

  eprintln!("{err_msg:?}");
  return MKError::new(MKErrorType::BlockingError, err_msg);
}

impl From<DieselError> for MKError {
  fn from(error: DieselError) -> Self {
    return MKError::new(MKErrorType::DBError, error.to_string());
  }
}
