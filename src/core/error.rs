use std::path::PathBuf;

#[derive(Debug, thiserror::Error)]
pub enum Error {
    #[error("io error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },

    #[error("json error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("vectory: {0}")]
    Vectory(String),

    #[error("rastera: {0}")]
    Rastera(String),

    #[error("uuid: {0}")]
    Uuid(#[from] uuid::Error),

    #[error("tar: {0}")]
    Tar(String),

    #[error("invalid zone: {0}")]
    InvalidZone(String),

    #[error("not found: {0}")]
    NotFound(String),

    #[error("boundary violation: child not contained in parent")]
    BoundaryViolation,

    #[error("{0}")]
    Msg(String),
}

impl Error {
    pub fn io(path: impl Into<PathBuf>, source: std::io::Error) -> Self {
        Self::Io {
            path: path.into(),
            source,
        }
    }

    pub fn msg(msg: impl Into<String>) -> Self {
        Self::Msg(msg.into())
    }
}

impl From<vectory::Error> for Error {
    fn from(e: vectory::Error) -> Self {
        Self::Vectory(e.to_string())
    }
}

impl From<rastera::Error> for Error {
    fn from(e: rastera::Error) -> Self {
        Self::Rastera(e.to_string())
    }
}

pub type Result<T> = std::result::Result<T, Error>;
