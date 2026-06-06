//! Raster container — a multi-layer `rastera::RasterCollection` plus
//! zoneout-flavoured metadata.
//!
//! Ported from `include/zoneout/zoneout/grid.hpp`.

use std::collections::HashMap;
use std::path::Path;

use datapod::{Geo, Pose};
use rastera::{GridData, Layer, RasterCollection, WriteOptions};
use uuid::Uuid;

use crate::core::error::{Error, Result};
use crate::core::meta::Meta;

const KEY_UUID: &str = "uuid";
const KEY_NAME: &str = "name";
const KEY_TYPE: &str = "type";
const KEY_SUBTYPE: &str = "subtype";

#[derive(Debug, Clone)]
pub struct Grid {
    meta: Meta,
    raster: RasterCollection,
}

impl Default for Grid {
    fn default() -> Self {
        Self::new("", "other", "default")
    }
}

impl Grid {
    pub fn new(
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
    ) -> Self {
        let mut g = Self {
            meta: Meta::new(name, kind).with_subtype(subtype),
            raster: RasterCollection {
                layers: Vec::new(),
                datum: Geo::new(0.0, 0.0, 0.0),
                shift: Pose::default(),
                resolution: 1.0,
            },
        };
        g.sync_meta_to_layers();
        g
    }

    pub fn with_datum(
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        datum: Geo,
    ) -> Self {
        let mut g = Self::new(name, kind, subtype);
        g.raster.datum = datum;
        g
    }

    pub fn with_spatial(
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        datum: Geo,
        shift: Pose,
        resolution: f64,
    ) -> Self {
        let mut g = Self::new(name, kind, subtype);
        g.raster.datum = datum;
        g.raster.shift = shift;
        g.raster.resolution = resolution;
        g
    }

    // -- identity -----------------------------------------------------------

    pub fn id(&self) -> Uuid {
        self.meta.id
    }
    pub fn name(&self) -> &str {
        &self.meta.name
    }
    pub fn kind(&self) -> &str {
        &self.meta.kind
    }
    pub fn subtype(&self) -> &str {
        &self.meta.subtype
    }
    pub fn meta(&self) -> &Meta {
        &self.meta
    }

    pub fn set_id(&mut self, id: Uuid) {
        self.meta.id = id;
        self.sync_meta_to_layers();
    }
    pub fn set_name(&mut self, name: impl Into<String>) {
        self.meta.name = name.into();
        self.sync_meta_to_layers();
    }
    pub fn set_kind(&mut self, kind: impl Into<String>) {
        self.meta.kind = kind.into();
        self.sync_meta_to_layers();
    }
    pub fn set_subtype(&mut self, subtype: impl Into<String>) {
        self.meta.subtype = subtype.into();
        self.sync_meta_to_layers();
    }

    // -- spatial ------------------------------------------------------------

    pub fn datum(&self) -> &Geo {
        &self.raster.datum
    }
    pub fn set_datum(&mut self, datum: Geo) {
        self.raster.datum = datum;
        for layer in &mut self.raster.layers {
            layer.datum = datum;
        }
    }

    pub fn shift(&self) -> &Pose {
        &self.raster.shift
    }
    pub fn set_shift(&mut self, shift: Pose) {
        self.raster.shift = shift;
        for layer in &mut self.raster.layers {
            layer.shift = shift;
        }
    }

    pub fn resolution(&self) -> f64 {
        self.raster.resolution
    }
    pub fn set_resolution(&mut self, resolution: f64) {
        self.raster.resolution = resolution;
        for layer in &mut self.raster.layers {
            layer.resolution = resolution;
        }
    }

    // -- raw access ---------------------------------------------------------

    pub fn raster(&self) -> &RasterCollection {
        &self.raster
    }
    pub fn raster_mut(&mut self) -> &mut RasterCollection {
        &mut self.raster
    }

    pub fn has_layers(&self) -> bool {
        !self.raster.layers.is_empty()
    }
    pub fn layer_count(&self) -> usize {
        self.raster.layers.len()
    }
    pub fn layers(&self) -> &[Layer] {
        &self.raster.layers
    }
    pub fn layers_mut(&mut self) -> &mut Vec<Layer> {
        &mut self.raster.layers
    }

    pub fn get_layer(&self, index: usize) -> Result<&Layer> {
        self.raster
            .layers
            .get(index)
            .ok_or_else(|| Error::NotFound(format!("layer index {index}")))
    }

    pub fn layer_by_name(&self, name: &str) -> Option<&Layer> {
        self.raster
            .layers
            .iter()
            .find(|l| l.get_global_properties().get(KEY_NAME).map(String::as_str) == Some(name))
    }

    pub fn layer_index_by_name(&self, name: &str) -> Option<usize> {
        self.raster
            .layers
            .iter()
            .position(|l| l.get_global_properties().get(KEY_NAME).map(String::as_str) == Some(name))
    }

    pub fn is_valid(&self) -> bool {
        self.has_layers() && !self.meta.name.is_empty()
    }

    // -- layer add / remove -------------------------------------------------

    /// Add an empty `u8` layer with the given pixel dimensions. Mirrors the
    /// C++ `add_grid(width, height, name, type, properties)` overload.
    pub fn add_empty_u8_layer(
        &mut self,
        width: usize,
        height: usize,
        name: impl Into<String>,
        kind: impl Into<String>,
        properties: HashMap<String, String>,
    ) -> &mut Layer {
        let grid = datapod::Grid {
            rows: height as u32,
            cols: width as u32,
            encoding: datapod::Encoding::U8,
            centered: 0,
            resolution: self.raster.resolution,
            pose: datapod::Pose::default(),
            data: vec![0u8; width * height],
        };
        self.add_layer(GridData::U8(grid), name, kind, properties)
    }

    pub fn add_layer(
        &mut self,
        grid: GridData,
        name: impl Into<String>,
        kind: impl Into<String>,
        properties: HashMap<String, String>,
    ) -> &mut Layer {
        let mut layer = Layer::new(grid);
        layer.datum = self.raster.datum;
        layer.shift = self.raster.shift;
        layer.resolution = self.raster.resolution;
        layer.set_global_property(KEY_NAME, &name.into());
        layer.set_global_property(KEY_TYPE, &kind.into());
        for (k, v) in properties {
            layer.set_global_property(&k, &v);
        }
        self.raster.layers.push(layer);
        self.sync_meta_to_layers();
        self.raster.layers.last_mut().expect("just pushed")
    }

    pub fn remove_layer(&mut self, index: usize) -> bool {
        if index < self.raster.layers.len() {
            self.raster.layers.remove(index);
            true
        } else {
            false
        }
    }

    pub fn remove_layer_by_name(&mut self, name: &str) -> bool {
        if let Some(idx) = self.layer_index_by_name(name) {
            self.raster.layers.remove(idx);
            true
        } else {
            false
        }
    }

    pub fn clear_layers(&mut self) {
        self.raster.layers.clear();
    }

    // -- I/O ----------------------------------------------------------------

    pub fn to_file(&self, path: impl AsRef<Path>) -> Result<()> {
        let mut staged = self.clone();
        staged.sync_meta_to_layers();
        rastera::writer::write_raster_collection(
            &staged.raster,
            path.as_ref(),
            &WriteOptions::default(),
        )?;
        Ok(())
    }

    pub fn from_file(path: impl AsRef<Path>) -> Result<Self> {
        let raster = rastera::parser::read_raster_collection(path.as_ref())?;
        Ok(Self::from_raster(raster))
    }

    pub fn from_raster(raster: RasterCollection) -> Self {
        let mut g = Self {
            meta: Meta::default(),
            raster,
        };
        g.extract_meta_from_layers();
        g
    }

    // -- internals ----------------------------------------------------------

    fn sync_meta_to_layers(&mut self) {
        let id_str = self.meta.id.hyphenated().to_string();
        for layer in &mut self.raster.layers {
            layer.set_global_property(KEY_UUID, &id_str);
            layer.set_global_property(KEY_NAME, &self.meta.name);
            layer.set_global_property(KEY_TYPE, &self.meta.kind);
            if !self.meta.subtype.is_empty() {
                layer.set_global_property(KEY_SUBTYPE, &self.meta.subtype);
            }
        }
    }

    fn extract_meta_from_layers(&mut self) {
        let Some(first) = self.raster.layers.first() else {
            return;
        };
        let props = first.get_global_properties();
        if let Some(id) = props.get(KEY_UUID).and_then(|s| Uuid::parse_str(s).ok()) {
            self.meta.id = id;
        }
        if let Some(name) = props.get(KEY_NAME) {
            self.meta.name = name.clone();
        }
        if let Some(kind) = props.get(KEY_TYPE) {
            self.meta.kind = kind.clone();
        }
        if let Some(subtype) = props.get(KEY_SUBTYPE) {
            self.meta.subtype = subtype.clone();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn grid_default_invalid() {
        let g = Grid::default();
        assert!(!g.is_valid());
        assert_eq!(g.layer_count(), 0);
    }

    #[test]
    fn grid_name_and_kind_set() {
        let g = Grid::new("alpha", "terrain", "default");
        assert_eq!(g.name(), "alpha");
        assert_eq!(g.kind(), "terrain");
    }
}
