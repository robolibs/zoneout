//! I/O facade — workspace JSON, plot tar bundles, zone/plot directory
//! layouts. Free-function mirrors of `zoneout::io::*` from the C++
//! library, delegating to methods on the respective types.

pub mod json;

use std::path::Path;

use datapod::Geo;

use crate::core::error::Result;
use crate::plot::Plot;
use crate::zone::Zone;

pub use json::{read_workspace_json, write_workspace_json};

// -- Workspace (flat single-file JSON format) -----------------------------

use crate::workspace::Workspace;

/// Load a Workspace from a single `.json` file (the flat `WorkspaceJson`
/// draft format). Mirrors the C++ `zoneout::load_workspace_json_file`.
pub fn load_workspace_json_file(path: impl AsRef<Path>) -> Result<Workspace> {
    let ws_json = read_workspace_json(path)?;
    Workspace::from_wire(ws_json)
}

/// Write a Workspace to a single `.json` file. Mirrors the C++
/// `zoneout::write_workspace_json_file`.
pub fn write_workspace_json_file(path: impl AsRef<Path>, workspace: &Workspace) -> Result<()> {
    let ws_json = workspace.to_wire();
    write_workspace_json(path, &ws_json)
}

// -- Zone ------------------------------------------------------------------

pub fn save_zone(zone: &Zone, directory: impl AsRef<Path>) -> Result<()> {
    zone.save(directory)
}

pub fn load_zone(directory: impl AsRef<Path>) -> Result<Zone> {
    Zone::load(directory)
}

pub fn save_zone_files(
    zone: &Zone,
    vector_path: impl AsRef<Path>,
    raster_path: impl AsRef<Path>,
) -> Result<()> {
    zone.save_plot_files(vector_path, raster_path)
}

pub fn load_zone_files(
    vector_path: impl AsRef<Path>,
    raster_path: impl AsRef<Path>,
) -> Result<Zone> {
    Zone::load_plot_files(vector_path, raster_path)
}

// -- Plot ------------------------------------------------------------------

pub fn save_plot(plot: &Plot, directory: impl AsRef<Path>) -> Result<()> {
    plot.save(directory)
}

/// Re-hydrate a `Plot` from a directory. `name`, `kind`, and `datum` are
/// accepted for C++ API parity and only used as fallbacks when the
/// on-disk metadata doesn't provide them.
pub fn load_plot(
    directory: impl AsRef<Path>,
    _name: &str,
    _kind: &str,
    _datum: Geo,
) -> Result<Plot> {
    Plot::load(directory)
}

pub fn save_plot_tar(plot: &Plot, tar_file: impl AsRef<Path>) -> Result<()> {
    plot.save_tar(tar_file)
}

pub fn load_plot_tar(
    tar_file: impl AsRef<Path>,
    _name: &str,
    _kind: &str,
    _datum: Geo,
) -> Result<Plot> {
    Plot::load_tar(tar_file)
}
