//! Poly + Grid coordination helpers (mirrors `polygrid.hpp`).
//!
//! `save_poly_grid` writes both halves of a Plot to adjacent files and
//! `load_poly_grid` reads them back, validating that (when both exist) their
//! UUIDs and names agree — the invariant the C++ code enforces.

use std::path::Path;

use datapod::{Aabb, Geo, Point, Polygon, Pose};
use rastera::GridData;
use vectory::Crs;

use crate::core::error::{Error, Result};
use crate::grid::Grid;
use crate::poly::Poly;

/// Rasterise the boundary of a zone onto a fresh `Grid` with one `base_layer`
/// of `u8` cells. Cells whose centre falls inside the boundary are set to
/// 255, all others to 0. Mirrors `make_base_grid` from `plot.hpp`.
pub fn make_base_grid(
    boundary: &Polygon,
    resolution: f64,
    datum: Geo,
    name: impl Into<String>,
    kind: impl Into<String>,
) -> Result<Grid> {
    if boundary.vertices.is_empty() {
        return Err(Error::InvalidZone("make_base_grid: empty boundary".into()));
    }
    if resolution <= 0.0 {
        return Err(Error::InvalidZone("make_base_grid: resolution must be > 0".into()));
    }

    let aabb = padded_aabb(boundary, resolution * 2.0);
    let min = aabb.min_point;
    let max = aabb.max_point;

    let width = ((max.x - min.x) / resolution).ceil().max(1.0) as usize;
    let height = ((max.y - min.y) / resolution).ceil().max(1.0) as usize;

    let mut cells = vec![0u8; width * height];
    for row in 0..height {
        for col in 0..width {
            let cx = min.x + (col as f64 + 0.5) * resolution;
            let cy = min.y + (row as f64 + 0.5) * resolution;
            if boundary.contains(Point::new(cx, cy, 0.0)) {
                cells[row * width + col] = 255;
            }
        }
    }

    let grid_data = datapod::Grid::<u8> {
        rows: height,
        cols: width,
        resolution,
        centered: false,
        pose: Pose::default(),
        data: cells.into(),
    };

    let shift = Pose {
        point: Point::new(min.x, min.y, 0.0),
        rotation: datapod::Quaternion::identity(),
    };

    let mut g = Grid::with_spatial(name, kind, "default", datum, shift, resolution);
    g.add_layer(
        GridData::U8(grid_data),
        "base_layer",
        "terrain",
        Default::default(),
    );
    Ok(g)
}

fn padded_aabb(boundary: &Polygon, padding: f64) -> Aabb {
    let mut aabb = boundary.get_aabb();
    aabb.min_point.x -= padding;
    aabb.min_point.y -= padding;
    aabb.max_point.x += padding;
    aabb.max_point.y += padding;
    aabb
}

/// Save a matched Poly + optional Grid pair to adjacent files.
pub fn save_poly_grid(
    poly: &Poly,
    grid: Option<&Grid>,
    vector_path: impl AsRef<Path>,
    raster_path: impl AsRef<Path>,
    crs: Crs,
) -> Result<()> {
    poly.to_file(vector_path, crs)?;
    if let Some(g) = grid {
        if g.has_layers() {
            g.to_file(raster_path)?;
        }
    }
    Ok(())
}

/// Load a Poly + optional Grid pair, verifying identity agreement when both
/// files are present.
pub fn load_poly_grid(
    vector_path: impl AsRef<Path>,
    raster_path: impl AsRef<Path>,
) -> Result<(Poly, Option<Grid>)> {
    let vec_path = vector_path.as_ref();
    let ras_path = raster_path.as_ref();

    let poly = if vec_path.exists() {
        Poly::from_file(vec_path)?
    } else {
        Poly::default()
    };
    let grid = if ras_path.exists() {
        Some(Grid::from_file(ras_path)?)
    } else {
        None
    };

    if vec_path.exists() {
        if let Some(g) = &grid {
            if poly.id() != g.id() {
                return Err(Error::InvalidZone(format!(
                    "poly / grid UUID mismatch: {} vs {}",
                    poly.id(), g.id()
                )));
            }
            if poly.name() != g.name() {
                return Err(Error::InvalidZone(format!(
                    "poly / grid name mismatch: '{}' vs '{}'",
                    poly.name(), g.name()
                )));
            }
        }
    }

    Ok((poly, grid))
}
