//! JSON read/write for `WorkspaceJson` (the on-disk wire format).

use std::fs;
use std::path::Path;

use crate::core::error::{Error, Result};
use crate::wire::WorkspaceJson;

pub fn read_workspace_json(path: impl AsRef<Path>) -> Result<WorkspaceJson> {
    let p = path.as_ref();
    let bytes = fs::read(p).map_err(|e| Error::io(p, e))?;
    let parsed: WorkspaceJson = serde_json::from_slice(&bytes)?;
    Ok(parsed)
}

pub fn write_workspace_json(path: impl AsRef<Path>, ws: &WorkspaceJson) -> Result<()> {
    let p = path.as_ref();
    let bytes = serde_json::to_vec_pretty(ws)?;
    fs::write(p, bytes).map_err(|e| Error::io(p, e))?;
    Ok(())
}
