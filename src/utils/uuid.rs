//! Thin UUID v4 helpers. Re-exports `uuid::Uuid` so downstream code only
//! needs one path.

pub use uuid::Uuid;

use crate::core::error::{Error, Result};

pub fn new_v4() -> Uuid {
    Uuid::new_v4()
}

pub fn parse(s: &str) -> Result<Uuid> {
    Uuid::parse_str(s).map_err(Error::from)
}

pub fn to_string(id: &Uuid) -> String {
    id.hyphenated().to_string()
}
