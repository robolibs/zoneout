//! `Plot` = `Poly` + optional `Grid`, with a shared identity and convenience
//! helpers for save / load (directory layout or single tar bundle).
//!
//! Ported from `include/zoneout/zoneout/plot.hpp`.

use std::collections::BTreeMap;
use std::fs::{self, File};
use std::io::Cursor;
use std::path::{Path, PathBuf};


use datapod::{Geo, Polygon};
use uuid::Uuid;
use vectory::Crs;

use crate::core::error::{Error, Result};
use crate::grid::Grid;
use crate::poly::Poly;
use crate::polygrid::{load_poly_grid, make_base_grid, save_poly_grid};

const PROP_PREFIX: &str = "prop_";
const FILE_VECTOR: &str = "vector.geojson";
const FILE_RASTER: &str = "raster.tiff";

#[derive(Debug, Clone)]
pub struct Plot {
    poly: Poly,
    grid: Option<Grid>,
}

impl Default for Plot {
    fn default() -> Self {
        Self { poly: Poly::default(), grid: None }
    }
}

impl Plot {
    pub fn new(poly: Poly) -> Self {
        Self { poly, grid: None }
    }

    pub fn with_grid(poly: Poly, grid: Grid) -> Self {
        Self { poly, grid: Some(grid) }
    }

    pub fn with_boundary(
        name: impl Into<String>,
        kind: impl Into<String>,
        boundary: Polygon,
        datum: Geo,
    ) -> Self {
        let name = name.into();
        let kind = kind.into();
        let mut poly = Poly::with_boundary(&name, &kind, "default", boundary);
        poly.set_datum(datum);
        Self::new(poly)
    }

    pub fn with_boundary_and_grid(
        name: impl Into<String>,
        kind: impl Into<String>,
        boundary: Polygon,
        datum: Geo,
        resolution: f64,
    ) -> Result<Self> {
        let name = name.into();
        let kind = kind.into();
        let mut grid = make_base_grid(&boundary, resolution, datum, &name, &kind)?;
        let mut poly = Poly::with_boundary(&name, &kind, "default", boundary);
        poly.set_datum(datum);
        grid.set_id(poly.id());
        Ok(Self::with_grid(poly, grid))
    }

    // -- identity passthrough ----------------------------------------------

    pub fn id(&self) -> Uuid { self.poly.id() }
    pub fn name(&self) -> &str { self.poly.name() }
    pub fn kind(&self) -> &str { self.poly.kind() }
    pub fn datum(&self) -> &Geo { self.poly.datum() }

    pub fn set_name(&mut self, name: impl Into<String>) {
        let name = name.into();
        self.poly.set_name(name.clone());
        if let Some(g) = &mut self.grid { g.set_name(name); }
    }
    pub fn set_kind(&mut self, kind: impl Into<String>) {
        let kind = kind.into();
        self.poly.set_kind(kind.clone());
        if let Some(g) = &mut self.grid { g.set_kind(kind); }
    }
    pub fn set_datum(&mut self, datum: Geo) {
        self.poly.set_datum(datum);
        if let Some(g) = &mut self.grid { g.set_datum(datum); }
    }

    // -- halves ------------------------------------------------------------

    pub fn poly(&self) -> &Poly { &self.poly }
    pub fn poly_mut(&mut self) -> &mut Poly { &mut self.poly }
    pub fn has_grid(&self) -> bool { self.grid.is_some() }
    pub fn grid(&self) -> Result<&Grid> {
        self.grid.as_ref().ok_or_else(|| Error::NotFound("plot has no grid".into()))
    }
    pub fn grid_mut(&mut self) -> Result<&mut Grid> {
        self.grid.as_mut().ok_or_else(|| Error::NotFound("plot has no grid".into()))
    }
    pub fn set_grid(&mut self, grid: Grid) { self.grid = Some(grid); }
    pub fn clear_grid(&mut self) { self.grid = None; }

    pub fn is_valid(&self) -> bool { self.poly.is_valid() }

    // -- plot-level properties (stored in poly.global_properties with "prop_" prefix) --

    pub fn set_property(&mut self, key: &str, value: impl Into<String>) {
        self.poly.set_global_property(format!("{PROP_PREFIX}{key}"), value);
    }
    pub fn property(&self, key: &str) -> Option<String> {
        self.poly.global_property(&format!("{PROP_PREFIX}{key}")).cloned()
    }
    pub fn has_property(&self, key: &str) -> bool {
        self.poly.has_global_property(&format!("{PROP_PREFIX}{key}"))
    }
    pub fn remove_property(&mut self, key: &str) -> bool {
        self.poly.remove_global_property(&format!("{PROP_PREFIX}{key}"))
    }
    pub fn properties(&self) -> BTreeMap<String, String> {
        self.poly
            .global_properties()
            .iter()
            .filter_map(|(k, v)| k.strip_prefix(PROP_PREFIX).map(|short| (short.to_string(), v.clone())))
            .collect()
    }
    pub fn clear_properties(&mut self) {
        let keys: Vec<String> = self
            .poly
            .global_properties()
            .keys()
            .filter(|k| k.starts_with(PROP_PREFIX))
            .cloned()
            .collect();
        for k in keys { self.poly.remove_global_property(&k); }
    }

    // -- I/O ---------------------------------------------------------------

    pub fn to_files(
        &self,
        vector_path: impl AsRef<Path>,
        raster_path: impl AsRef<Path>,
    ) -> Result<()> {
        save_poly_grid(&self.poly, self.grid.as_ref(), vector_path, raster_path, Crs::Wgs)
    }

    pub fn save(&self, directory: impl AsRef<Path>) -> Result<()> {
        let dir = directory.as_ref();
        fs::create_dir_all(dir).map_err(|e| Error::io(dir, e))?;
        self.to_files(dir.join(FILE_VECTOR), dir.join(FILE_RASTER))
    }

    pub fn from_files(
        vector_path: impl AsRef<Path>,
        raster_path: impl AsRef<Path>,
    ) -> Result<Self> {
        let (poly, grid) = load_poly_grid(vector_path, raster_path)?;
        Ok(Self { poly, grid })
    }

    pub fn load(directory: impl AsRef<Path>) -> Result<Self> {
        let dir = directory.as_ref();
        Self::from_files(dir.join(FILE_VECTOR), dir.join(FILE_RASTER))
    }

    // -- tar bundle --------------------------------------------------------
    //
    // Tar layout: two members at the root — `vector.geojson` and
    // optionally `raster.tiff`. Matches the microtar bundle written by the
    // C++ version.

    pub fn save_tar(&self, path: impl AsRef<Path>) -> Result<()> {
        let tmp = tempdir_for_plot()?;
        self.save(&tmp)?;

        let tar_path = path.as_ref();
        let file = File::create(tar_path).map_err(|e| Error::io(tar_path, e))?;
        let mut builder = tar::Builder::new(file);

        let vec_path = tmp.join(FILE_VECTOR);
        if vec_path.exists() {
            builder
                .append_path_with_name(&vec_path, FILE_VECTOR)
                .map_err(|e| Error::Tar(e.to_string()))?;
        }
        let ras_path = tmp.join(FILE_RASTER);
        if ras_path.exists() {
            builder
                .append_path_with_name(&ras_path, FILE_RASTER)
                .map_err(|e| Error::Tar(e.to_string()))?;
        }
        builder.finish().map_err(|e| Error::Tar(e.to_string()))?;
        let _ = fs::remove_dir_all(&tmp);
        Ok(())
    }

    pub fn load_tar(path: impl AsRef<Path>) -> Result<Self> {
        let tar_path = path.as_ref();
        let bytes = fs::read(tar_path).map_err(|e| Error::io(tar_path, e))?;
        let mut archive = tar::Archive::new(Cursor::new(bytes));

        let tmp = tempdir_for_plot()?;
        archive.unpack(&tmp).map_err(|e| Error::Tar(e.to_string()))?;
        let plot = Self::load(&tmp)?;
        let _ = fs::remove_dir_all(&tmp);
        Ok(plot)
    }
}

fn tempdir_for_plot() -> Result<PathBuf> {
    let mut path = std::env::temp_dir();
    let unique = format!("zoneout-plot-{}", Uuid::new_v4().simple());
    path.push(unique);
    fs::create_dir_all(&path).map_err(|e| Error::io(&path, e))?;
    Ok(path)
}

// -- Builder ---------------------------------------------------------------

#[derive(Debug, Default)]
pub struct PlotBuilder {
    name: Option<String>,
    kind: Option<String>,
    boundary: Option<Polygon>,
    datum: Option<Geo>,
    resolution: Option<f64>,
    properties: BTreeMap<String, String>,
}

impl PlotBuilder {
    pub fn new() -> Self { Self::default() }

    pub fn with_name(mut self, name: impl Into<String>) -> Self {
        self.name = Some(name.into());
        self
    }
    pub fn with_kind(mut self, kind: impl Into<String>) -> Self {
        self.kind = Some(kind.into());
        self
    }
    pub fn with_boundary(mut self, b: Polygon) -> Self {
        self.boundary = Some(b);
        self
    }
    pub fn with_datum(mut self, d: Geo) -> Self {
        self.datum = Some(d);
        self
    }
    pub fn with_resolution(mut self, r: f64) -> Self {
        self.resolution = Some(r);
        self
    }
    pub fn with_property(mut self, k: impl Into<String>, v: impl Into<String>) -> Self {
        self.properties.insert(k.into(), v.into());
        self
    }

    pub fn build(self) -> Result<Plot> {
        let name = self.name.unwrap_or_default();
        let kind = self.kind.unwrap_or_else(|| "other".into());
        let boundary = self.boundary.unwrap_or_default();
        let datum = self.datum.unwrap_or_else(|| Geo::new(0.0, 0.0, 0.0));

        let mut plot = match self.resolution {
            Some(r) if !boundary.empty() => {
                Plot::with_boundary_and_grid(&name, &kind, boundary, datum, r)?
            }
            _ => Plot::with_boundary(&name, &kind, boundary, datum),
        };
        for (k, v) in self.properties { plot.set_property(&k, v); }
        Ok(plot)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use datapod::Point;

    fn square() -> Polygon {
        Polygon {
            vertices: vec![
                Point::new(0.0, 0.0, 0.0),
                Point::new(10.0, 0.0, 0.0),
                Point::new(10.0, 10.0, 0.0),
                Point::new(0.0, 10.0, 0.0),
            ]
            .into(),
        }
    }

    #[test]
    fn plot_builder_no_grid() {
        let p = PlotBuilder::new()
            .with_name("field1")
            .with_kind("field")
            .with_boundary(square())
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_property("crop", "wheat")
            .build()
            .expect("build");

        assert_eq!(p.name(), "field1");
        assert!(!p.has_grid());
        assert_eq!(p.property("crop").as_deref(), Some("wheat"));
    }

    #[test]
    fn plot_builder_with_grid() {
        let p = PlotBuilder::new()
            .with_name("field2")
            .with_kind("field")
            .with_boundary(square())
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_resolution(1.0)
            .build()
            .expect("build");

        assert!(p.has_grid());
        assert_eq!(p.grid().unwrap().name(), "field2");
        assert_eq!(p.id(), p.grid().unwrap().id());
    }

    #[test]
    fn plot_property_prefix_roundtrip() {
        let mut p = PlotBuilder::new()
            .with_name("f")
            .with_kind("field")
            .build()
            .expect("build");

        p.set_property("a", "1");
        p.set_property("b", "2");
        let props = p.properties();
        assert_eq!(props.get("a").map(String::as_str), Some("1"));
        assert_eq!(props.get("b").map(String::as_str), Some("2"));
    }
}
