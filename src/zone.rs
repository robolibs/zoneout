//! `Zone` — hierarchical region with a Plot, properties, child zones, and
//! per-zone node-ID attachments. Ported from `include/zoneout/zoneout/zone.hpp`.

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;


use datapod::{Aabb, Geo, Point, Polygon};
use rastera::{GridData, Layer};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::core::error::{Error, Result};
use crate::grid::Grid;
use crate::plot::Plot;
use crate::poly::{LineElement, PointElement, PolygonElement};

const FILE_VECTOR: &str = "vector.geojson";
const FILE_RASTER: &str = "raster.tiff";
const FILE_ZONE_JSON: &str = "zone.json";
const DIR_CHILD_PREFIX: &str = "child_";

#[derive(Debug, Clone)]
pub struct Zone {
    plot_data: Plot,
    id: Uuid,
    name: String,
    kind: String,
    properties: BTreeMap<String, String>,
    node_ids: Vec<Uuid>,
    children: Vec<Zone>,
}

impl Zone {
    pub fn new(
        name: impl Into<String>,
        kind: impl Into<String>,
        boundary: Polygon,
        datum: Geo,
        resolution: f64,
    ) -> Result<Self> {
        let name = name.into();
        let kind = kind.into();
        let plot = if resolution > 0.0 && !boundary.empty() {
            Plot::with_boundary_and_grid(&name, &kind, boundary, datum, resolution)?
        } else {
            Plot::with_boundary(&name, &kind, boundary, datum)
        };
        Ok(Self::from_plot_internal(name, kind, plot))
    }

    pub fn with_plot(name: impl Into<String>, kind: impl Into<String>, plot: Plot) -> Self {
        Self::from_plot_internal(name.into(), kind.into(), plot)
    }

    pub fn with_initial_grid(
        name: impl Into<String>,
        kind: impl Into<String>,
        boundary: Polygon,
        mut grid: Grid,
        datum: Geo,
    ) -> Self {
        let name = name.into();
        let kind = kind.into();
        let mut plot = Plot::with_boundary(&name, &kind, boundary, datum);
        grid.set_id(plot.id());
        grid.set_name(name.clone());
        grid.set_kind(kind.clone());
        grid.set_datum(datum);
        plot.set_grid(grid);
        Self::from_plot_internal(name, kind, plot)
    }

    fn from_plot_internal(name: String, kind: String, mut plot: Plot) -> Self {
        plot.set_name(name.clone());
        plot.set_kind(kind.clone());
        Self {
            id: plot.id(),
            name,
            kind,
            plot_data: plot,
            properties: BTreeMap::new(),
            node_ids: Vec::new(),
            children: Vec::new(),
        }
    }

    // -- identity -----------------------------------------------------------

    pub fn id(&self) -> Uuid { self.id }
    pub fn name(&self) -> &str { &self.name }
    pub fn kind(&self) -> &str { &self.kind }

    pub fn set_id(&mut self, id: Uuid) {
        self.id = id;
        self.plot_data.poly_mut().set_id(id);
    }
    pub fn set_name(&mut self, name: impl Into<String>) {
        self.name = name.into();
        self.plot_data.set_name(&self.name);
    }
    pub fn set_kind(&mut self, kind: impl Into<String>) {
        self.kind = kind.into();
        self.plot_data.set_kind(&self.kind);
    }
    pub fn set_datum(&mut self, datum: Geo) {
        self.plot_data.set_datum(datum);
    }

    pub fn datum(&self) -> &Geo { self.plot_data.datum() }
    pub fn plot(&self) -> &Plot { &self.plot_data }
    pub fn plot_mut(&mut self) -> &mut Plot { &mut self.plot_data }
    pub fn poly(&self) -> &crate::poly::Poly { self.plot_data.poly() }
    pub fn has_grid(&self) -> bool { self.plot_data.has_grid() }

    // -- properties ---------------------------------------------------------

    pub fn set_property(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.properties.insert(key.into(), value.into());
    }
    pub fn property(&self, key: &str) -> Option<&String> { self.properties.get(key) }
    pub fn properties(&self) -> &BTreeMap<String, String> { &self.properties }
    pub fn properties_mut(&mut self) -> &mut BTreeMap<String, String> { &mut self.properties }
    pub fn has_property(&self, key: &str) -> bool { self.properties.contains_key(key) }
    pub fn remove_property(&mut self, key: &str) -> bool {
        self.properties.remove(key).is_some()
    }
    pub fn clear_properties(&mut self) { self.properties.clear(); }

    // -- node ids -----------------------------------------------------------

    pub fn node_ids(&self) -> &[Uuid] { &self.node_ids }
    pub fn node_ids_mut(&mut self) -> &mut Vec<Uuid> { &mut self.node_ids }
    pub fn set_node_ids(&mut self, ids: Vec<Uuid>) { self.node_ids = ids; }
    pub fn clear_node_ids(&mut self) { self.node_ids.clear(); }

    /// Recursively clear `node_ids` on this zone and every descendant.
    pub fn visit_clear_node_ids(&mut self) {
        self.node_ids.clear();
        for c in &mut self.children { c.visit_clear_node_ids(); }
    }

    // -- children / tree ---------------------------------------------------

    pub fn children(&self) -> &[Zone] { &self.children }
    pub fn children_mut(&mut self) -> &mut Vec<Zone> { &mut self.children }
    pub fn child_count(&self) -> usize { self.children.len() }

    pub fn add_child(&mut self, child: Zone) -> Result<()> {
        self.validate_child_boundary(&child)?;
        if self.find(child.id).is_some() {
            return Err(Error::InvalidZone(format!("duplicate child id: {}", child.id)));
        }
        self.children.push(child);
        Ok(())
    }

    /// Recursive depth-first removal. Returns `true` if the zone was found
    /// and removed, mirroring the C++ semantics.
    pub fn remove_child(&mut self, child_id: Uuid) -> bool {
        let before = self.children.len();
        self.children.retain(|c| c.id != child_id);
        if self.children.len() != before {
            return true;
        }
        for c in &mut self.children {
            if c.remove_child(child_id) {
                return true;
            }
        }
        false
    }

    pub fn find(&self, zone_id: Uuid) -> Option<&Zone> {
        if self.id == zone_id { return Some(self); }
        for c in &self.children {
            if let Some(z) = c.find(zone_id) { return Some(z); }
        }
        None
    }

    pub fn find_mut(&mut self, zone_id: Uuid) -> Option<&mut Zone> {
        if self.id == zone_id { return Some(self); }
        for c in &mut self.children {
            if let Some(z) = c.find_mut(zone_id) { return Some(z); }
        }
        None
    }

    pub fn find_by_name(&self, name: &str) -> Option<&Zone> {
        if self.name == name { return Some(self); }
        for c in &self.children {
            if let Some(z) = c.find_by_name(name) { return Some(z); }
        }
        None
    }

    /// Post-order tree visitor, passing each zone and its depth from `self`.
    pub fn visit<F: FnMut(&Zone, usize)>(&self, f: &mut F) {
        self.visit_at(f, 0);
    }

    fn visit_at<F: FnMut(&Zone, usize)>(&self, f: &mut F, depth: usize) {
        for c in &self.children {
            c.visit_at(f, depth + 1);
        }
        f(self, depth);
    }

    pub fn depth_of(&self, zone_id: Uuid) -> Option<usize> {
        self.depth_of_rec(zone_id, 0)
    }

    fn depth_of_rec(&self, zone_id: Uuid, depth: usize) -> Option<usize> {
        if self.id == zone_id { return Some(depth); }
        for c in &self.children {
            if let Some(d) = c.depth_of_rec(zone_id, depth + 1) { return Some(d); }
        }
        None
    }

    // -- spatial ------------------------------------------------------------

    pub fn contains(&self, point: Point) -> bool { self.plot_data.poly().contains(point) }

    pub fn bounding_box(&self) -> Option<Aabb> { self.plot_data.poly().bounding_box() }

    pub fn polygon_elements_in_area(&self, bbox: Aabb) -> Vec<PolygonElement> {
        self.plot_data
            .poly()
            .polygon_elements()
            .iter()
            .filter(|e| bbox.intersects(e.geometry.get_aabb()))
            .cloned()
            .collect()
    }

    pub fn point_elements_in_area(&self, bbox: Aabb) -> Vec<PointElement> {
        self.plot_data
            .poly()
            .point_elements()
            .iter()
            .filter(|e| bbox.contains(e.geometry))
            .cloned()
            .collect()
    }

    pub fn line_elements_in_area(&self, bbox: Aabb) -> Vec<LineElement> {
        self.plot_data
            .poly()
            .line_elements()
            .iter()
            .filter(|e| bbox.contains(e.geometry.start) || bbox.contains(e.geometry.end))
            .cloned()
            .collect()
    }

    pub fn points_in_polygon(&self, area: &Polygon) -> Vec<PointElement> {
        self.plot_data
            .poly()
            .point_elements()
            .iter()
            .filter(|e| area.contains(e.geometry))
            .cloned()
            .collect()
    }

    pub fn is_valid(&self) -> bool { self.plot_data.is_valid() }

    /// Human-readable summary of the grid (layer count, first layer size).
    pub fn raster_info(&self) -> String {
        match self.plot_data.grid() {
            Ok(g) if g.has_layers() => {
                let n = g.layer_count();
                if let Some(first) = g.layers().first() {
                    format!("Raster size: {}x{} ({n} layer{})",
                        first.width(), first.height(),
                        if n == 1 { "" } else { "s" })
                } else {
                    format!("Raster: {n} layers")
                }
            }
            _ => "No raster".to_string(),
        }
    }

    /// Human-readable summary of vector elements in this zone.
    pub fn element_info(&self) -> String {
        let p = self.plot_data.poly();
        let np = p.polygon_elements().len();
        let nl = p.line_elements().len();
        let npt = p.point_elements().len();
        format!(
            "Elements: {np} polygon{}, {nl} line{}, {npt} point{} ({} total)",
            if np == 1 { "" } else { "s" },
            if nl == 1 { "" } else { "s" },
            if npt == 1 { "" } else { "s" },
            np + nl + npt,
        )
    }

    /// Push this zone's identity onto its `Plot` (normally done automatically
    /// on setters; exposed for callers who mutate the plot directly).
    pub fn sync_to_plot(&mut self) {
        self.plot_data.set_name(self.name.clone());
        self.plot_data.set_kind(self.kind.clone());
        self.plot_data.poly_mut().set_id(self.id);
    }

    // -- raster / polygon additions ----------------------------------------

    pub fn add_raster_layer(
        &mut self,
        grid: GridData,
        name: impl Into<String>,
        kind: impl Into<String>,
        properties: std::collections::HashMap<String, String>,
    ) -> Result<()> {
        let grid_ref = match self.plot_data.grid_mut() {
            Ok(g) => g,
            Err(_) => {
                // create an empty Grid hanging off this zone's datum
                let datum = *self.plot_data.datum();
                let fresh = Grid::with_datum(&self.name, &self.kind, "default", datum);
                self.plot_data.set_grid(fresh);
                self.plot_data.grid_mut()?
            }
        };
        grid_ref.add_layer(grid, name, kind, properties);
        Ok(())
    }

    pub fn add_polygon_element(
        &mut self,
        geometry: Polygon,
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        properties: BTreeMap<String, String>,
    ) -> Result<Uuid> {
        if !self.plot_data.poly().has_field_boundary() {
            return Err(Error::InvalidZone(
                "zone has no boundary; cannot attach polygon element".into(),
            ));
        }
        // verify every vertex is inside the boundary
        for v in geometry.iter() {
            if !self.plot_data.poly().contains(*v) {
                return Err(Error::BoundaryViolation);
            }
        }

        // Rasterise onto the first grid layer (if any) to mirror the C++
        // behaviour: cells whose centre falls inside `geometry` are painted
        // with a random "colour" value in [50, 200]. RGBA layers are left
        // alone — only scalar layers are painted.
        let color: u8 = {
            use std::time::{SystemTime, UNIX_EPOCH};
            let seed = SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.subsec_nanos())
                .unwrap_or(0);
            50 + (seed % 151) as u8 // 50..=200
        };
        if let Ok(g) = self.plot_data.grid_mut() {
            if let Some(layer) = g.layers_mut().first_mut() {
                rasterise_polygon_onto_layer(&geometry, color, layer);
            }
        }

        let id = Uuid::new_v4();
        self.plot_data.poly_mut().add_polygon_element(
            id,
            name,
            kind,
            subtype,
            geometry,
            properties,
        );
        Ok(id)
    }

    // -- save / load (recursive directory layout) --------------------------

    pub fn save(&self, directory: impl AsRef<Path>) -> Result<()> {
        let dir = directory.as_ref();
        fs::create_dir_all(dir).map_err(|e| Error::io(dir, e))?;

        self.plot_data
            .to_files(dir.join(FILE_VECTOR), dir.join(FILE_RASTER))?;

        let meta = ZoneMetaOnDisk::from(self);
        let meta_path = dir.join(FILE_ZONE_JSON);
        let json = serde_json::to_string_pretty(&meta)?;
        fs::write(&meta_path, json).map_err(|e| Error::io(&meta_path, e))?;

        for (i, child) in self.children.iter().enumerate() {
            child.save(dir.join(format!("{DIR_CHILD_PREFIX}{i}")))?;
        }
        Ok(())
    }

    pub fn load(directory: impl AsRef<Path>) -> Result<Self> {
        let dir = directory.as_ref();
        let plot = Plot::from_files(dir.join(FILE_VECTOR), dir.join(FILE_RASTER))?;

        let meta_path = dir.join(FILE_ZONE_JSON);
        let meta: ZoneMetaOnDisk = if meta_path.exists() {
            let bytes = fs::read(&meta_path).map_err(|e| Error::io(&meta_path, e))?;
            serde_json::from_slice(&bytes)?
        } else {
            ZoneMetaOnDisk::default_from_plot(&plot)
        };

        let mut zone = Self::with_plot(&meta.name, &meta.kind, plot);
        zone.set_id(meta.id);
        zone.properties = meta.properties;
        zone.node_ids = meta.node_ids;

        let mut index = 0;
        loop {
            let child_dir = dir.join(format!("{DIR_CHILD_PREFIX}{index}"));
            if !child_dir.is_dir() { break; }
            let child = Zone::load(&child_dir)?;
            zone.children.push(child);
            index += 1;
        }
        Ok(zone)
    }

    pub fn save_plot_files(
        &self,
        vector_path: impl AsRef<Path>,
        raster_path: impl AsRef<Path>,
    ) -> Result<()> {
        self.plot_data.to_files(vector_path, raster_path)
    }

    pub fn load_plot_files(
        vector_path: impl AsRef<Path>,
        raster_path: impl AsRef<Path>,
    ) -> Result<Self> {
        let plot = Plot::from_files(vector_path, raster_path)?;
        let name = plot.name().to_string();
        let kind = plot.kind().to_string();
        Ok(Self::with_plot(name, kind, plot))
    }

    // -- internals ----------------------------------------------------------

    fn validate_child_boundary(&self, child: &Zone) -> Result<()> {
        let parent_poly = self.plot_data.poly();
        if !parent_poly.has_field_boundary() { return Ok(()); }
        let child_poly = child.plot_data.poly();
        if !child_poly.has_field_boundary() { return Ok(()); }
        for v in child_poly.field_boundary().iter() {
            if !parent_poly.contains(*v) {
                return Err(Error::BoundaryViolation);
            }
        }
        Ok(())
    }
}

/// Free-function factory mirroring C++ `zoneout::make_zone`. Equivalent to
/// `Zone::new` but exported at the crate root for C++-to-Rust migrations.
pub fn make_zone(
    name: impl Into<String>,
    kind: impl Into<String>,
    boundary: Polygon,
    datum: Geo,
    resolution: f64,
) -> Result<Zone> {
    Zone::new(name, kind, boundary, datum, resolution)
}

fn rasterise_polygon_onto_layer(geometry: &Polygon, color: u8, layer: &mut Layer) {
    macro_rules! paint_numeric {
        ($g:expr, $cast:ty) => {{
            let g: &mut datapod::Grid = $g;
            let rows = g.rows as usize;
            let cols = g.cols as usize;
            // Precompute world-space cell centers before taking the mut
            // borrow of `g.data` (avoids overlapping borrows).
            let mut points: Vec<datapod::Point> = Vec::with_capacity(rows * cols);
            for r in 0..rows {
                for c in 0..cols {
                    points.push(g.get_point(r, c));
                }
            }
            let typed: &mut [$cast] = bytemuck::cast_slice_mut(&mut g.data);
            for r in 0..rows {
                for c in 0..cols {
                    if geometry.contains(points[r * cols + c]) {
                        typed[r * cols + c] = color as $cast;
                    }
                }
            }
        }};
    }

    match &mut layer.grid {
        GridData::U8(g) => paint_numeric!(g, u8),
        GridData::I8(g) => paint_numeric!(g, i8),
        GridData::U16(g) => paint_numeric!(g, u16),
        GridData::I16(g) => paint_numeric!(g, i16),
        GridData::U32(g) => paint_numeric!(g, u32),
        GridData::I32(g) => paint_numeric!(g, i32),
        GridData::F32(g) => paint_numeric!(g, f32),
        GridData::F64(g) => paint_numeric!(g, f64),
        GridData::Rgba8(_) => { /* skip RGBA layers to match C++ behaviour */ }
    }
}

// -- on-disk zone.json ------------------------------------------------------

#[derive(Debug, Serialize, Deserialize)]
struct ZoneMetaOnDisk {
    id: Uuid,
    name: String,
    #[serde(rename = "type")]
    kind: String,
    #[serde(default)]
    properties: BTreeMap<String, String>,
    #[serde(default)]
    node_ids: Vec<Uuid>,
}

impl ZoneMetaOnDisk {
    fn from(zone: &Zone) -> Self {
        Self {
            id: zone.id,
            name: zone.name.clone(),
            kind: zone.kind.clone(),
            properties: zone.properties.clone(),
            node_ids: zone.node_ids.clone(),
        }
    }

    fn default_from_plot(plot: &Plot) -> Self {
        Self {
            id: plot.id(),
            name: plot.name().to_string(),
            kind: plot.kind().to_string(),
            properties: BTreeMap::new(),
            node_ids: Vec::new(),
        }
    }
}

// -- Builder ---------------------------------------------------------------

#[derive(Debug, Default)]
pub struct ZoneBuilder {
    name: Option<String>,
    kind: Option<String>,
    boundary: Option<Polygon>,
    datum: Option<Geo>,
    resolution: f64,
    initial_grid: Option<Grid>,
    properties: BTreeMap<String, String>,
    raster_layers: Vec<(GridData, String, String, std::collections::HashMap<String, String>)>,
    polygon_elements: Vec<(Polygon, String, String, String, BTreeMap<String, String>)>,
}

impl ZoneBuilder {
    pub fn new() -> Self {
        Self { resolution: 1.0, ..Self::default() }
    }

    pub fn with_name(mut self, n: impl Into<String>) -> Self { self.name = Some(n.into()); self }
    pub fn with_kind(mut self, k: impl Into<String>) -> Self { self.kind = Some(k.into()); self }
    pub fn with_boundary(mut self, b: Polygon) -> Self { self.boundary = Some(b); self }
    pub fn with_datum(mut self, d: Geo) -> Self { self.datum = Some(d); self }
    pub fn with_resolution(mut self, r: f64) -> Self { self.resolution = r; self }
    pub fn with_initial_grid(mut self, g: Grid) -> Self { self.initial_grid = Some(g); self }
    pub fn with_property(mut self, k: impl Into<String>, v: impl Into<String>) -> Self {
        self.properties.insert(k.into(), v.into()); self
    }
    pub fn with_raster_layer(
        mut self,
        data: GridData,
        name: impl Into<String>,
        kind: impl Into<String>,
        props: std::collections::HashMap<String, String>,
    ) -> Self {
        self.raster_layers.push((data, name.into(), kind.into(), props));
        self
    }
    pub fn with_polygon_element(
        mut self,
        geom: Polygon,
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        props: BTreeMap<String, String>,
    ) -> Self {
        self.polygon_elements.push((geom, name.into(), kind.into(), subtype.into(), props));
        self
    }

    pub fn validation_error(&self) -> Option<String> {
        if self.name.is_none() { return Some("name is required".into()); }
        if self.kind.is_none() { return Some("type is required".into()); }
        if self.boundary.is_none() { return Some("boundary is required".into()); }
        if self.datum.is_none() { return Some("datum is required".into()); }
        None
    }

    pub fn is_valid(&self) -> bool { self.validation_error().is_none() }

    pub fn build(self) -> Result<Zone> {
        if let Some(e) = self.validation_error() {
            return Err(Error::InvalidZone(e));
        }
        let name = self.name.unwrap();
        let kind = self.kind.unwrap();
        let boundary = self.boundary.unwrap();
        let datum = self.datum.unwrap();

        let mut zone = if let Some(g) = self.initial_grid {
            Zone::with_initial_grid(&name, &kind, boundary, g, datum)
        } else {
            Zone::new(&name, &kind, boundary, datum, self.resolution)?
        };

        for (k, v) in self.properties { zone.set_property(k, v); }

        for (data, n, k, props) in self.raster_layers {
            zone.add_raster_layer(data, n, k, props)?;
        }

        for (geom, n, k, s, props) in self.polygon_elements {
            zone.add_polygon_element(geom, n, k, s, props)?;
        }

        Ok(zone)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn square(size: f64, offset: (f64, f64)) -> Polygon {
        Polygon {
            vertices: vec![
                Point::new(offset.0, offset.1, 0.0),
                Point::new(offset.0 + size, offset.1, 0.0),
                Point::new(offset.0 + size, offset.1 + size, 0.0),
                Point::new(offset.0, offset.1 + size, 0.0),
            ]
            .into(),
        }
    }

    #[test]
    fn zone_builder_minimal() {
        let z = ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(100.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .expect("build");
        assert_eq!(z.name(), "farm");
        assert!(z.is_valid());
        assert_eq!(z.child_count(), 0);
    }

    #[test]
    fn zone_add_child_inside_boundary() {
        let mut parent = ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(100.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        let child = ZoneBuilder::new()
            .with_name("field_a")
            .with_kind("field")
            .with_boundary(square(10.0, (5.0, 5.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        let child_id = child.id();
        parent.add_child(child).expect("inside");
        assert_eq!(parent.child_count(), 1);
        assert!(parent.find(child_id).is_some());
        assert_eq!(parent.depth_of(child_id), Some(1));
    }

    #[test]
    fn zone_add_child_outside_rejected() {
        let mut parent = ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(10.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        let child = ZoneBuilder::new()
            .with_name("outside")
            .with_kind("field")
            .with_boundary(square(10.0, (50.0, 50.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        let err = parent.add_child(child).unwrap_err();
        assert!(matches!(err, Error::BoundaryViolation));
    }

    #[test]
    fn zone_find_by_name_recursive() {
        let mut parent = ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(100.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        let a = ZoneBuilder::new()
            .with_name("a")
            .with_kind("field")
            .with_boundary(square(10.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        parent.add_child(a).unwrap();
        assert!(parent.find_by_name("a").is_some());
        assert!(parent.find_by_name("zz").is_none());
    }

    #[test]
    fn zone_add_polygon_element_paints_grid() {
        let mut z = ZoneBuilder::new()
            .with_name("f")
            .with_kind("field")
            .with_boundary(square(10.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_resolution(1.0) // auto-creates a u8 base grid
            .build()
            .unwrap();

        assert!(z.has_grid());
        // Add a small polygon inside the boundary — the grid's base layer
        // should gain a non-zero painted region.
        let inner = square(4.0, (2.0, 2.0));
        z.add_polygon_element(inner, "rock", "obstacle", "default", BTreeMap::new())
            .unwrap();

        // The first layer should now contain at least one cell with the
        // painted value (50..=200).
        let g = z.plot().grid().unwrap();
        let layer = &g.layers()[0];
        let painted_any = match &layer.grid {
            GridData::U8(data) => data.data.iter().any(|&v| (50..=200).contains(&v)),
            _ => false,
        };
        assert!(painted_any, "rasterise_polygon_onto_layer failed to paint any cell");
    }

    #[test]
    fn zone_raster_and_element_info() {
        let no_grid = ZoneBuilder::new()
            .with_name("f")
            .with_kind("field")
            .with_boundary(square(10.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_resolution(0.0)
            .build()
            .unwrap();
        assert_eq!(no_grid.raster_info(), "No raster");
        assert!(no_grid.element_info().contains("0 polygons"));

        let with_grid = ZoneBuilder::new()
            .with_name("g")
            .with_kind("field")
            .with_boundary(square(10.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_resolution(1.0)
            .build()
            .unwrap();
        assert!(with_grid.raster_info().starts_with("Raster size:"));
    }

    #[test]
    fn zone_save_and_load_roundtrip() {
        let mut parent = ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(100.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_property("owner", "alice")
            .build()
            .unwrap();
        let child = ZoneBuilder::new()
            .with_name("a")
            .with_kind("field")
            .with_boundary(square(10.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap();
        parent.add_child(child).unwrap();

        let mut dir = std::env::temp_dir();
        dir.push(format!("zoneout-test-{}", Uuid::new_v4().simple()));
        parent.save(&dir).expect("save");

        let loaded = Zone::load(&dir).expect("load");
        assert_eq!(loaded.name(), "farm");
        assert_eq!(loaded.property("owner").map(String::as_str), Some("alice"));
        assert_eq!(loaded.child_count(), 1);
        assert_eq!(loaded.children()[0].name(), "a");

        let _ = fs::remove_dir_all(&dir);
    }
}
