//! Flat JSON "wire" structures that mirror `include/zoneout/zoneout/wire.hpp`.
//!
//! These structs are byte-for-byte compatible (field names and shape) with
//! the C++ serialisation so that workspaces written by either side can be
//! loaded by the other.

use std::collections::BTreeMap;


use concord::{Enu, to_enu, to_wgs_from_enu};
use datapod::{Geo, Point, Polygon};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

/// Wire-friendly mirror of `datapod::Geo` (lat / lon / alt). We avoid a
/// direct `datapod::Geo` on the wire because the upstream type does not
/// derive serde.
#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize, PartialEq)]
pub struct JsonGeo {
    #[serde(default)]
    pub lat: f64,
    #[serde(default)]
    pub lon: f64,
    #[serde(default)]
    pub alt: f64,
}

impl From<datapod::Geo> for JsonGeo {
    fn from(g: datapod::Geo) -> Self {
        Self { lat: g.latitude, lon: g.longitude, alt: g.altitude }
    }
}

impl From<JsonGeo> for datapod::Geo {
    fn from(g: JsonGeo) -> Self { datapod::Geo::new(g.lat, g.lon, g.alt) }
}

/// `global` (WGS84 lat/lon) vs `local` (ENU x/y) wire encoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum CoordMode {
    Global,
    Local,
}

impl Default for CoordMode {
    fn default() -> Self { Self::Global }
}

/// A 2D point in whichever frame `CoordMode` selects.
#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize, PartialEq)]
pub struct JsonPoint {
    #[serde(default)]
    pub lat: f64,
    #[serde(default)]
    pub lon: f64,
}

impl JsonPoint {
    pub const fn new(lat: f64, lon: f64) -> Self { Self { lat, lon } }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ZoneJson {
    pub id: Uuid,
    #[serde(default = "default_zone_name")]
    pub name: String,
    #[serde(rename = "type", default = "default_zone_type")]
    pub kind: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub parent_id: Option<Uuid>,
    #[serde(default)]
    pub child_ids: Vec<Uuid>,
    #[serde(default)]
    pub node_ids: Vec<Uuid>,
    #[serde(default)]
    pub properties: BTreeMap<String, String>,
    #[serde(default)]
    pub polygon_latlon: Vec<JsonPoint>,
    #[serde(default)]
    pub grid_enabled: bool,
    #[serde(default = "default_resolution")]
    pub grid_resolution: f64,
}

impl Default for ZoneJson {
    fn default() -> Self {
        Self {
            id: Uuid::new_v4(),
            name: default_zone_name(),
            kind: default_zone_type(),
            parent_id: None,
            child_ids: Vec::new(),
            node_ids: Vec::new(),
            properties: BTreeMap::new(),
            polygon_latlon: Vec::new(),
            grid_enabled: false,
            grid_resolution: 1.0,
        }
    }
}

fn default_zone_name() -> String { "Zone".into() }
fn default_zone_type() -> String { "zone".into() }
fn default_resolution() -> f64 { 1.0 }

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeJson {
    pub id: Uuid,
    #[serde(default = "default_node_name")]
    pub name: String,
    #[serde(default)]
    pub latlon: JsonPoint,
    #[serde(default)]
    pub zone_ids: Vec<Uuid>,
    #[serde(default)]
    pub properties: BTreeMap<String, String>,
}

impl Default for NodeJson {
    fn default() -> Self {
        Self {
            id: Uuid::new_v4(),
            name: default_node_name(),
            latlon: JsonPoint::default(),
            zone_ids: Vec::new(),
            properties: BTreeMap::new(),
        }
    }
}

fn default_node_name() -> String { "Node".into() }

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeJson {
    pub id: Uuid,
    pub source_id: Uuid,
    pub target_id: Uuid,
    #[serde(default)]
    pub directed: bool,
    #[serde(default = "default_weight")]
    pub weight: f64,
    #[serde(default)]
    pub zone_ids: Vec<Uuid>,
    #[serde(default)]
    pub properties: BTreeMap<String, String>,
}

fn default_weight() -> f64 { 1.0 }

#[derive(Debug, Clone)]
pub struct WorkspaceJson {
    pub root_zone_id: Uuid,
    pub coord_mode: CoordMode,
    pub ref_: Option<JsonPoint>,
    pub datum: Option<JsonGeo>,
    pub zones: BTreeMap<Uuid, ZoneJson>,
    pub nodes: BTreeMap<Uuid, NodeJson>,
    pub edges: BTreeMap<Uuid, EdgeJson>,
    pub name: String,
}

fn default_workspace_name() -> String { "Workspace".into() }

impl Default for WorkspaceJson {
    fn default() -> Self {
        Self {
            root_zone_id: Uuid::nil(),
            coord_mode: CoordMode::default(),
            ref_: None,
            datum: None,
            zones: BTreeMap::new(),
            nodes: BTreeMap::new(),
            edges: BTreeMap::new(),
            name: default_workspace_name(),
        }
    }
}

// `ref` is a reserved keyword; the C++ JSON key is "ref", so rename the
// field at (de)serialisation time.
mod serde_rename {
    use super::{JsonPoint, WorkspaceJson};
    use serde::{Deserialize, Deserializer, Serialize, Serializer};
    use serde::ser::SerializeStruct;

    impl Serialize for WorkspaceJson {
        fn serialize<S: Serializer>(&self, s: S) -> Result<S::Ok, S::Error> {
            let mut st = s.serialize_struct("WorkspaceJson", 8)?;
            st.serialize_field("root_zone_id", &self.root_zone_id)?;
            st.serialize_field("coord_mode", &self.coord_mode)?;
            if let Some(r) = &self.ref_ { st.serialize_field("ref", r)?; }
            if let Some(d) = &self.datum { st.serialize_field("datum", d)?; }
            st.serialize_field("zones", &self.zones)?;
            st.serialize_field("nodes", &self.nodes)?;
            st.serialize_field("edges", &self.edges)?;
            st.serialize_field("name", &self.name)?;
            st.end()
        }
    }

    #[derive(Deserialize)]
    struct Helper {
        #[serde(default)]
        root_zone_id: super::Uuid,
        #[serde(default)]
        coord_mode: super::CoordMode,
        #[serde(default, rename = "ref")]
        ref_: Option<JsonPoint>,
        #[serde(default)]
        datum: Option<super::JsonGeo>,
        #[serde(default)]
        zones: std::collections::BTreeMap<super::Uuid, super::ZoneJson>,
        #[serde(default)]
        nodes: std::collections::BTreeMap<super::Uuid, super::NodeJson>,
        #[serde(default)]
        edges: std::collections::BTreeMap<super::Uuid, super::EdgeJson>,
        #[serde(default = "super::default_workspace_name")]
        name: String,
    }

    impl<'de> Deserialize<'de> for WorkspaceJson {
        fn deserialize<D: Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
            let h = Helper::deserialize(d)?;
            Ok(Self {
                root_zone_id: h.root_zone_id,
                coord_mode: h.coord_mode,
                ref_: h.ref_,
                datum: h.datum,
                zones: h.zones,
                nodes: h.nodes,
                edges: h.edges,
                name: h.name,
            })
        }
    }
}

// --- coord-mode-aware point helpers --------------------------------------

pub fn valid_latlon(p: JsonPoint) -> bool {
    p.lat.is_finite() && p.lon.is_finite() && (-90.0..=90.0).contains(&p.lat) && (-180.0..=180.0).contains(&p.lon)
}

pub fn valid_local_xy(p: JsonPoint) -> bool {
    p.lat.is_finite() && p.lon.is_finite()
}

/// JsonPoint → local 3D point. `mode=Local` treats the JsonPoint as
/// `{lat: y, lon: x}` (the C++ convention); `mode=Global` runs a proper
/// WGS→ENU conversion via concord.
pub fn to_local_point(p: JsonPoint, datum: Geo, mode: CoordMode) -> Point {
    match mode {
        CoordMode::Local => Point::new(p.lon, p.lat, 0.0),
        CoordMode::Global => {
            let wgs = Geo::new(p.lat, p.lon, 0.0);
            let enu = to_enu(datum, wgs);
            Point::new(enu.east(), enu.north(), enu.up())
        }
    }
}

pub fn to_json_point(p: Point, datum: Geo, mode: CoordMode) -> JsonPoint {
    match mode {
        CoordMode::Local => JsonPoint { lat: p.y, lon: p.x },
        CoordMode::Global => {
            let wgs = to_wgs_from_enu(Enu::new(p.x, p.y, p.z, datum));
            JsonPoint { lat: wgs.latitude, lon: wgs.longitude }
        }
    }
}

pub fn to_local_polygon(poly: &[JsonPoint], datum: Geo, mode: CoordMode) -> Polygon {
    let vertices: Vec<Point> = poly.iter().map(|p| to_local_point(*p, datum, mode)).collect();
    Polygon::new(vertices)
}

pub fn to_json_polygon(poly: &Polygon, datum: Geo, mode: CoordMode) -> Vec<JsonPoint> {
    let mut out: Vec<JsonPoint> = poly.vertices.iter().map(|v| to_json_point(*v, datum, mode)).collect();
    // C++ drops the closing duplicate vertex if the ring is closed.
    if out.len() > 1 && out.first() == out.last() {
        out.pop();
    }
    out
}

// --- datum / root inference ----------------------------------------------

pub fn infer_datum(ws: &WorkspaceJson) -> Result<Geo, String> {
    if let Some(r) = ws.ref_ { return Ok(Geo::new(r.lat, r.lon, 0.0)); }
    if let Some(d) = ws.datum { return Ok(Geo::new(d.lat, d.lon, d.alt)); }
    for zone in ws.zones.values() {
        if let Some(p) = zone.polygon_latlon.first() {
            return Ok(Geo::new(p.lat, p.lon, 0.0));
        }
    }
    for node in ws.nodes.values() {
        return Ok(Geo::new(node.latlon.lat, node.latlon.lon, 0.0));
    }
    if ws.coord_mode == CoordMode::Local {
        return Err("Cannot infer workspace datum for local coordinates without a ref point".into());
    }
    Err("Cannot infer workspace datum from empty draft".into())
}

pub fn infer_root_zone_id(ws: &WorkspaceJson) -> Result<Uuid, String> {
    if !ws.root_zone_id.is_nil() { return Ok(ws.root_zone_id); }
    let mut candidate: Option<Uuid> = None;
    for (id, z) in &ws.zones {
        if z.parent_id.is_none() {
            if candidate.is_some() {
                return Err("Draft has multiple root-zone candidates and no root_zone_id".into());
            }
            candidate = Some(*id);
        }
    }
    candidate.ok_or_else(|| "Draft has no root zone".into())
}

// --- validation ----------------------------------------------------------

/// Structural check over a draft `WorkspaceJson`. Returns the list of
/// errors; empty = valid. Mirrors `zoneout::validate_workspace_json`.
pub fn validate_workspace_json(ws: &WorkspaceJson) -> Vec<String> {
    let mut errors = Vec::new();

    for (id, zone) in &ws.zones {
        if *id != zone.id {
            errors.push(format!("Zone map key '{id}' does not match zone.id '{}'", zone.id));
        }
        if zone.polygon_latlon.len() < 3 {
            errors.push(format!("Zone '{id}' must have at least 3 polygon vertices"));
        }
        let point_valid = |p: JsonPoint| match ws.coord_mode {
            CoordMode::Local => valid_local_xy(p),
            CoordMode::Global => valid_latlon(p),
        };
        for v in &zone.polygon_latlon {
            if !point_valid(*v) {
                errors.push(format!("Zone '{id}' has invalid polygon vertex"));
                break;
            }
        }
        if let Some(parent_id) = zone.parent_id {
            match ws.zones.get(&parent_id) {
                None => errors.push(format!("Zone '{id}' points to missing parent '{parent_id}'")),
                Some(parent) => {
                    if !parent.child_ids.contains(id) {
                        errors.push(format!(
                            "Zone '{id}' points to parent '{parent_id}' but is missing from that parent's child_ids"
                        ));
                    }
                }
            }
        }
        for node_id in &zone.node_ids {
            if !ws.nodes.contains_key(node_id) {
                errors.push(format!("Zone '{id}' references missing node '{node_id}'"));
            }
        }
        if zone.grid_enabled && zone.grid_resolution <= 0.0 {
            errors.push(format!("Zone '{id}' has grid_enabled but non-positive grid_resolution"));
        }
    }

    for (id, node) in &ws.nodes {
        if *id != node.id {
            errors.push(format!("Node map key '{id}' does not match node.id '{}'", node.id));
        }
        let node_valid = match ws.coord_mode {
            CoordMode::Local => valid_local_xy(node.latlon),
            CoordMode::Global => valid_latlon(node.latlon),
        };
        if !node_valid {
            errors.push(format!("Node '{id}' has invalid coordinates"));
        }
        for zone_id in &node.zone_ids {
            if !ws.zones.contains_key(zone_id) {
                errors.push(format!("Node '{id}' references missing zone '{zone_id}'"));
            }
        }
    }

    for (id, edge) in &ws.edges {
        if *id != edge.id {
            errors.push(format!("Edge map key '{id}' does not match edge.id '{}'", edge.id));
        }
        if !edge.weight.is_finite() {
            errors.push(format!("Edge '{id}' has non-finite weight"));
        }
        if !ws.nodes.contains_key(&edge.source_id) {
            errors.push(format!("Edge '{id}' references unknown source node '{}'", edge.source_id));
        }
        if !ws.nodes.contains_key(&edge.target_id) {
            errors.push(format!("Edge '{id}' references unknown target node '{}'", edge.target_id));
        }
        for zone_id in &edge.zone_ids {
            if !ws.zones.contains_key(zone_id) {
                errors.push(format!("Edge '{id}' references missing zone '{zone_id}'"));
            }
        }
    }

    errors
}

/// Throw (`Err`) if validation reports any issues. Mirrors the C++
/// `require_valid_workspace_json`.
pub fn require_valid_workspace_json(ws: &WorkspaceJson) -> Result<(), String> {
    let errors = validate_workspace_json(ws);
    if errors.is_empty() { return Ok(()); }
    let mut msg = String::from("WorkspaceJson validation failed:");
    for e in errors { msg.push_str("\n- "); msg.push_str(&e); }
    Err(msg)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn coord_mode_serialises_lowercase() {
        let g = serde_json::to_string(&CoordMode::Global).unwrap();
        let l = serde_json::to_string(&CoordMode::Local).unwrap();
        assert_eq!(g, "\"global\"");
        assert_eq!(l, "\"local\"");
    }

    #[test]
    fn zone_json_uses_type_key() {
        let z = ZoneJson::default();
        let v = serde_json::to_value(&z).unwrap();
        assert!(v.get("type").is_some());
        assert!(v.get("kind").is_none());
    }

    #[test]
    fn workspace_json_uses_ref_key() {
        let mut w = WorkspaceJson::default();
        w.ref_ = Some(JsonPoint::new(52.0, 5.0));
        let v = serde_json::to_value(&w).unwrap();
        assert!(v.get("ref").is_some());
        assert_eq!(v["ref"]["lat"], 52.0);
    }

    #[test]
    fn workspace_json_roundtrip() {
        let mut w = WorkspaceJson::default();
        w.root_zone_id = Uuid::new_v4();
        w.coord_mode = CoordMode::Local;
        let s = serde_json::to_string(&w).unwrap();
        let back: WorkspaceJson = serde_json::from_str(&s).unwrap();
        assert_eq!(back.root_zone_id, w.root_zone_id);
        assert_eq!(back.coord_mode, CoordMode::Local);
    }
}
