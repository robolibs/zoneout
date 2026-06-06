//! Vector container — boundary polygon plus typed feature elements.
//!
//! Ported from `include/zoneout/zoneout/poly.hpp`. The C++ code keeps two
//! parallel views of the data: a `vectkit::FeatureCollection` (the canonical
//! serialisation), and three typed vectors (`polygon_elements`,
//! `line_elements`, `point_elements`) for fast typed access. This port does
//! the same, rebuilding the typed vectors from the collection on load and
//! keeping them in lock-step on mutation.

use std::collections::{BTreeMap, HashMap};
use std::path::Path;

use datapod::{Aabb, Euler, Geo, Point, Polygon, Segment};
use serde::{Deserialize, Serialize};
use uuid::Uuid;
use vectory::{Crs, Feature, FeatureCollection, Geometry};

use crate::core::error::Result;
use crate::core::meta::Meta;

const KEY_UUID: &str = "uuid";
const KEY_NAME: &str = "name";
const KEY_TYPE: &str = "type";
const KEY_SUBTYPE: &str = "subtype";
const KEY_BORDER: &str = "border";

/// Metadata block stored on each structured feature.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StructuredElement {
    pub id: Uuid,
    pub name: String,
    #[serde(rename = "type")]
    pub kind: String,
    #[serde(default)]
    pub subtype: String,
    #[serde(default)]
    pub properties: BTreeMap<String, String>,
}

impl StructuredElement {
    pub fn new(
        id: Uuid,
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        properties: BTreeMap<String, String>,
    ) -> Self {
        Self {
            id,
            name: name.into(),
            kind: kind.into(),
            subtype: subtype.into(),
            properties,
        }
    }

    fn from_feature(feat: &Feature) -> Option<Self> {
        let props = &feat.properties;
        let id = props.get(KEY_UUID).and_then(|s| Uuid::parse_str(s).ok())?;
        let name = props.get(KEY_NAME).cloned()?;
        let kind = props.get(KEY_TYPE).cloned()?;
        let subtype = props.get(KEY_SUBTYPE).cloned().unwrap_or_default();
        let mut extras = BTreeMap::new();
        for (k, v) in props {
            if matches!(
                k.as_str(),
                KEY_UUID | KEY_NAME | KEY_TYPE | KEY_SUBTYPE | KEY_BORDER
            ) {
                continue;
            }
            extras.insert(k.clone(), v.clone());
        }
        Some(Self {
            id,
            name,
            kind,
            subtype,
            properties: extras,
        })
    }

    fn write_properties(&self, out: &mut HashMap<String, String>) {
        out.insert(KEY_UUID.into(), self.id.hyphenated().to_string());
        out.insert(KEY_NAME.into(), self.name.clone());
        out.insert(KEY_TYPE.into(), self.kind.clone());
        if !self.subtype.is_empty() {
            out.insert(KEY_SUBTYPE.into(), self.subtype.clone());
        }
        for (k, v) in &self.properties {
            out.insert(k.clone(), v.clone());
        }
    }
}

#[derive(Debug, Clone)]
pub struct PolygonElement {
    pub meta: StructuredElement,
    pub geometry: Polygon,
}

#[derive(Debug, Clone)]
pub struct LineElement {
    pub meta: StructuredElement,
    pub geometry: Segment,
}

#[derive(Debug, Clone)]
pub struct PointElement {
    pub meta: StructuredElement,
    pub geometry: Point,
}

/// Vector / GeoJSON container for a single zone.
#[derive(Debug, Clone)]
pub struct Poly {
    meta: Meta,
    collection: FeatureCollection,
    field_boundary: Polygon,
    polygon_elements: Vec<PolygonElement>,
    line_elements: Vec<LineElement>,
    point_elements: Vec<PointElement>,
}

impl Default for Poly {
    fn default() -> Self {
        Self::new("", "other", "default")
    }
}

impl Poly {
    pub fn new(
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
    ) -> Self {
        let mut poly = Self {
            meta: Meta::new(name, kind).with_subtype(subtype),
            collection: FeatureCollection::new(Geo::new(0.0, 0.0, 0.0), Euler::default()),
            field_boundary: Polygon::default(),
            polygon_elements: Vec::new(),
            line_elements: Vec::new(),
            point_elements: Vec::new(),
        };
        poly.sync_meta_to_globals();
        poly
    }

    pub fn with_boundary(
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        boundary: Polygon,
    ) -> Self {
        let mut poly = Self::new(name, kind, subtype);
        poly.field_boundary = boundary;
        poly
    }

    pub fn with_spatial(
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        boundary: Polygon,
        datum: Geo,
        heading: Euler,
    ) -> Self {
        let mut poly = Self::with_boundary(name, kind, subtype, boundary);
        poly.collection.datum = datum;
        poly.collection.heading = heading;
        poly
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
        self.sync_meta_to_globals();
    }
    pub fn set_name(&mut self, name: impl Into<String>) {
        self.meta.name = name.into();
        self.sync_meta_to_globals();
    }
    pub fn set_kind(&mut self, kind: impl Into<String>) {
        self.meta.kind = kind.into();
        self.sync_meta_to_globals();
    }
    pub fn set_subtype(&mut self, subtype: impl Into<String>) {
        self.meta.subtype = subtype.into();
        self.sync_meta_to_globals();
    }

    // -- spatial ------------------------------------------------------------

    pub fn datum(&self) -> &Geo {
        &self.collection.datum
    }
    pub fn set_datum(&mut self, datum: Geo) {
        self.collection.datum = datum;
    }
    pub fn heading(&self) -> &Euler {
        &self.collection.heading
    }
    pub fn set_heading(&mut self, heading: Euler) {
        self.collection.heading = heading;
    }

    pub fn field_boundary(&self) -> &Polygon {
        &self.field_boundary
    }
    pub fn set_field_boundary(&mut self, boundary: Polygon) {
        self.field_boundary = boundary;
    }
    pub fn has_field_boundary(&self) -> bool {
        !self.field_boundary.empty()
    }

    pub fn area(&self) -> f64 {
        self.field_boundary.area()
    }
    pub fn perimeter(&self) -> f64 {
        self.field_boundary.perimeter()
    }
    pub fn contains(&self, p: Point) -> bool {
        self.has_field_boundary() && self.field_boundary.contains(p)
    }
    pub fn bounding_box(&self) -> Option<Aabb> {
        if self.has_field_boundary() {
            Some(self.field_boundary.get_aabb())
        } else {
            None
        }
    }

    pub fn is_valid(&self) -> bool {
        self.has_field_boundary() && !self.meta.name.is_empty()
    }

    // -- global properties --------------------------------------------------

    pub fn global_properties(&self) -> &HashMap<String, String> {
        &self.collection.global_properties
    }
    pub fn set_global_property(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.collection
            .global_properties
            .insert(key.into(), value.into());
    }
    pub fn global_property(&self, key: &str) -> Option<&String> {
        self.collection.global_properties.get(key)
    }
    pub fn has_global_property(&self, key: &str) -> bool {
        self.collection.global_properties.contains_key(key)
    }
    pub fn remove_global_property(&mut self, key: &str) -> bool {
        self.collection.global_properties.remove(key).is_some()
    }
    pub fn clear_global_properties(&mut self) {
        self.collection.global_properties.clear();
        self.sync_meta_to_globals();
    }

    // -- raw feature access -------------------------------------------------

    pub fn collection(&self) -> &FeatureCollection {
        &self.collection
    }
    pub fn collection_mut(&mut self) -> &mut FeatureCollection {
        &mut self.collection
    }
    pub fn feature_count(&self) -> usize {
        self.collection.features.len()
    }

    pub fn add_feature(&mut self, feature: Feature) {
        self.collection.features.push(feature);
    }

    pub fn get_feature(&self, index: usize) -> Option<&Feature> {
        self.collection.features.get(index)
    }

    /// Attach a property to the boundary feature (the one tagged
    /// `border="true"`). Creates the boundary feature if it hasn't been
    /// staged yet. Mirrors C++ `Poly::set_field_property`.
    pub fn set_field_property(&mut self, key: impl Into<String>, value: impl Into<String>) {
        self.stage_boundary_feature();
        let key = key.into();
        let value = value.into();
        for f in &mut self.collection.features {
            if f.properties
                .get(KEY_BORDER)
                .map(|s| s == "true")
                .unwrap_or(false)
            {
                f.properties.insert(key, value);
                return;
            }
        }
    }

    // -- element add/remove -------------------------------------------------

    pub fn add_polygon_element(
        &mut self,
        id: Uuid,
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        geometry: Polygon,
        properties: BTreeMap<String, String>,
    ) {
        let meta = StructuredElement::new(id, name, kind, subtype, properties);
        let feature = Self::build_feature(&meta, Geometry::Polygon(geometry.clone()));
        self.collection.features.push(feature);
        self.polygon_elements
            .push(PolygonElement { meta, geometry });
    }

    pub fn add_polygon(&mut self, geometry: Polygon, kind: impl Into<String>) -> Uuid {
        self.add_polygon_typed(geometry, kind, BTreeMap::new())
    }

    /// Short-form with properties. Auto-generates a UUID and uses `kind`
    /// as the name; subtype defaults to `"default"`. Mirrors the C++
    /// `add_polygon_element(geometry, type, props)` overload.
    pub fn add_polygon_typed(
        &mut self,
        geometry: Polygon,
        kind: impl Into<String>,
        properties: BTreeMap<String, String>,
    ) -> Uuid {
        let id = Uuid::new_v4();
        let kind = kind.into();
        self.add_polygon_element(id, kind.clone(), kind, "default", geometry, properties);
        id
    }

    pub fn add_line_element(
        &mut self,
        id: Uuid,
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        geometry: Segment,
        properties: BTreeMap<String, String>,
    ) {
        let meta = StructuredElement::new(id, name, kind, subtype, properties);
        let feature = Self::build_feature(&meta, Geometry::Segment(geometry));
        self.collection.features.push(feature);
        self.line_elements.push(LineElement { meta, geometry });
    }

    pub fn add_line(&mut self, geometry: Segment, kind: impl Into<String>) -> Uuid {
        self.add_line_typed(geometry, kind, BTreeMap::new())
    }

    pub fn add_line_typed(
        &mut self,
        geometry: Segment,
        kind: impl Into<String>,
        properties: BTreeMap<String, String>,
    ) -> Uuid {
        let id = Uuid::new_v4();
        let kind = kind.into();
        self.add_line_element(id, kind.clone(), kind, "default", geometry, properties);
        id
    }

    pub fn add_point_element(
        &mut self,
        id: Uuid,
        name: impl Into<String>,
        kind: impl Into<String>,
        subtype: impl Into<String>,
        geometry: Point,
        properties: BTreeMap<String, String>,
    ) {
        let meta = StructuredElement::new(id, name, kind, subtype, properties);
        let feature = Self::build_feature(&meta, Geometry::Point(geometry));
        self.collection.features.push(feature);
        self.point_elements.push(PointElement { meta, geometry });
    }

    pub fn add_point(&mut self, geometry: Point, kind: impl Into<String>) -> Uuid {
        self.add_point_typed(geometry, kind, BTreeMap::new())
    }

    pub fn add_point_typed(
        &mut self,
        geometry: Point,
        kind: impl Into<String>,
        properties: BTreeMap<String, String>,
    ) -> Uuid {
        let id = Uuid::new_v4();
        let kind = kind.into();
        self.add_point_element(id, kind.clone(), kind, "default", geometry, properties);
        id
    }

    pub fn remove_polygon_element(&mut self, id: Uuid) -> bool {
        let before = self.polygon_elements.len();
        self.polygon_elements.retain(|e| e.meta.id != id);
        if self.polygon_elements.len() != before {
            self.drop_features_by_ids(&[id]);
            true
        } else {
            false
        }
    }

    pub fn remove_line_element(&mut self, id: Uuid) -> bool {
        let before = self.line_elements.len();
        self.line_elements.retain(|e| e.meta.id != id);
        if self.line_elements.len() != before {
            self.drop_features_by_ids(&[id]);
            true
        } else {
            false
        }
    }

    pub fn remove_point_element(&mut self, id: Uuid) -> bool {
        let before = self.point_elements.len();
        self.point_elements.retain(|e| e.meta.id != id);
        if self.point_elements.len() != before {
            self.drop_features_by_ids(&[id]);
            true
        } else {
            false
        }
    }

    pub fn polygon_elements(&self) -> &[PolygonElement] {
        &self.polygon_elements
    }
    pub fn line_elements(&self) -> &[LineElement] {
        &self.line_elements
    }
    pub fn point_elements(&self) -> &[PointElement] {
        &self.point_elements
    }

    pub fn polygon_element(&self, id: Uuid) -> Option<&PolygonElement> {
        self.polygon_elements.iter().find(|e| e.meta.id == id)
    }
    pub fn line_element(&self, id: Uuid) -> Option<&LineElement> {
        self.line_elements.iter().find(|e| e.meta.id == id)
    }
    pub fn point_element(&self, id: Uuid) -> Option<&PointElement> {
        self.point_elements.iter().find(|e| e.meta.id == id)
    }

    pub fn polygons_by_type(&self, kind: &str) -> Vec<&PolygonElement> {
        self.polygon_elements
            .iter()
            .filter(|e| e.meta.kind == kind)
            .collect()
    }
    pub fn lines_by_type(&self, kind: &str) -> Vec<&LineElement> {
        self.line_elements
            .iter()
            .filter(|e| e.meta.kind == kind)
            .collect()
    }
    pub fn points_by_type(&self, kind: &str) -> Vec<&PointElement> {
        self.point_elements
            .iter()
            .filter(|e| e.meta.kind == kind)
            .collect()
    }
    pub fn polygons_by_subtype(&self, subtype: &str) -> Vec<&PolygonElement> {
        self.polygon_elements
            .iter()
            .filter(|e| e.meta.subtype == subtype)
            .collect()
    }
    pub fn lines_by_subtype(&self, subtype: &str) -> Vec<&LineElement> {
        self.line_elements
            .iter()
            .filter(|e| e.meta.subtype == subtype)
            .collect()
    }
    pub fn points_by_subtype(&self, subtype: &str) -> Vec<&PointElement> {
        self.point_elements
            .iter()
            .filter(|e| e.meta.subtype == subtype)
            .collect()
    }

    pub fn clear_polygon_elements(&mut self) {
        let ids: Vec<Uuid> = self.polygon_elements.iter().map(|e| e.meta.id).collect();
        self.polygon_elements.clear();
        self.drop_features_by_ids(&ids);
    }
    pub fn clear_line_elements(&mut self) {
        let ids: Vec<Uuid> = self.line_elements.iter().map(|e| e.meta.id).collect();
        self.line_elements.clear();
        self.drop_features_by_ids(&ids);
    }
    pub fn clear_point_elements(&mut self) {
        let ids: Vec<Uuid> = self.point_elements.iter().map(|e| e.meta.id).collect();
        self.point_elements.clear();
        self.drop_features_by_ids(&ids);
    }
    pub fn clear_all_elements(&mut self) {
        self.polygon_elements.clear();
        self.line_elements.clear();
        self.point_elements.clear();
        self.collection.features.retain(|f| {
            f.properties
                .get(KEY_BORDER)
                .map(|s| s == "true")
                .unwrap_or(false)
        });
    }

    // -- I/O ----------------------------------------------------------------

    pub fn to_file(&self, path: impl AsRef<Path>, crs: Crs) -> Result<()> {
        let mut staged = self.clone();
        staged.stage_boundary_feature();
        vectory::io::write(&staged.collection, path.as_ref(), crs)?;
        Ok(())
    }

    pub fn from_file(path: impl AsRef<Path>) -> Result<Self> {
        let collection = vectory::io::read(path.as_ref())?;
        Ok(Self::from_collection(collection))
    }

    pub fn from_collection(collection: FeatureCollection) -> Self {
        let mut poly = Self {
            meta: Meta::default(),
            collection,
            field_boundary: Polygon::default(),
            polygon_elements: Vec::new(),
            line_elements: Vec::new(),
            point_elements: Vec::new(),
        };
        poly.extract_meta_from_globals();
        poly.load_structured_elements();
        poly
    }

    // -- internals ----------------------------------------------------------

    fn build_feature(meta: &StructuredElement, geom: Geometry) -> Feature {
        let mut properties = HashMap::new();
        meta.write_properties(&mut properties);
        Feature {
            geometry: geom,
            properties,
        }
    }

    fn sync_meta_to_globals(&mut self) {
        let g = &mut self.collection.global_properties;
        g.insert(KEY_UUID.into(), self.meta.id.hyphenated().to_string());
        g.insert(KEY_NAME.into(), self.meta.name.clone());
        g.insert(KEY_TYPE.into(), self.meta.kind.clone());
        if !self.meta.subtype.is_empty() {
            g.insert(KEY_SUBTYPE.into(), self.meta.subtype.clone());
        }
    }

    fn extract_meta_from_globals(&mut self) {
        let g = &self.collection.global_properties;
        if let Some(id) = g.get(KEY_UUID).and_then(|s| Uuid::parse_str(s).ok()) {
            self.meta.id = id;
        }
        if let Some(name) = g.get(KEY_NAME) {
            self.meta.name = name.clone();
        }
        if let Some(kind) = g.get(KEY_TYPE) {
            self.meta.kind = kind.clone();
        }
        if let Some(subtype) = g.get(KEY_SUBTYPE) {
            self.meta.subtype = subtype.clone();
        }
    }

    fn load_structured_elements(&mut self) {
        self.polygon_elements.clear();
        self.line_elements.clear();
        self.point_elements.clear();

        for feat in &self.collection.features {
            let is_border = feat
                .properties
                .get(KEY_BORDER)
                .map(|s| s == "true")
                .unwrap_or(false);
            match &feat.geometry {
                Geometry::Polygon(poly) if is_border => {
                    self.field_boundary = poly.clone();
                }
                Geometry::Polygon(poly) => {
                    if let Some(meta) = StructuredElement::from_feature(feat) {
                        self.polygon_elements.push(PolygonElement {
                            meta,
                            geometry: poly.clone(),
                        });
                    }
                }
                Geometry::Segment(s) => {
                    if let Some(meta) = StructuredElement::from_feature(feat) {
                        self.line_elements.push(LineElement { meta, geometry: *s });
                    }
                }
                Geometry::Point(p) => {
                    if let Some(meta) = StructuredElement::from_feature(feat) {
                        self.point_elements
                            .push(PointElement { meta, geometry: *p });
                    }
                }
                Geometry::Path(_) => { /* unsupported as a typed element */ }
            }
        }
    }

    fn stage_boundary_feature(&mut self) {
        if !self.has_field_boundary() {
            return;
        }
        let already = self.collection.features.iter().any(|f| {
            f.properties
                .get(KEY_BORDER)
                .map(|s| s == "true")
                .unwrap_or(false)
        });
        if already {
            return;
        }
        let mut properties = HashMap::new();
        properties.insert(KEY_BORDER.into(), "true".into());
        properties.insert(KEY_NAME.into(), self.meta.name.clone());
        properties.insert(KEY_TYPE.into(), self.meta.kind.clone());
        properties.insert(KEY_UUID.into(), self.meta.id.hyphenated().to_string());
        self.collection.features.push(Feature {
            geometry: Geometry::Polygon(self.field_boundary.clone()),
            properties,
        });
    }

    fn drop_features_by_ids(&mut self, ids: &[Uuid]) {
        let set: std::collections::HashSet<String> =
            ids.iter().map(|u| u.hyphenated().to_string()).collect();
        self.collection
            .features
            .retain(|f| match f.properties.get(KEY_UUID) {
                Some(uuid_str) => !set.contains(uuid_str),
                None => true,
            });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
    fn poly_boundary_area_perimeter() {
        let p = Poly::with_boundary("f1", "field", "default", square());
        assert!(p.has_field_boundary());
        assert!((p.area() - 100.0).abs() < 1e-9);
        assert!((p.perimeter() - 40.0).abs() < 1e-9);
        assert!(p.contains(Point::new(5.0, 5.0, 0.0)));
    }

    #[test]
    fn poly_add_and_find_elements() {
        let mut p = Poly::new("f1", "field", "default");
        let id = p.add_point(Point::new(1.0, 2.0, 0.0), "obstacle");
        assert_eq!(p.point_elements().len(), 1);
        assert!(p.point_element(id).is_some());
        assert_eq!(p.points_by_type("obstacle").len(), 1);
    }

    #[test]
    fn poly_sync_meta_to_globals() {
        let p = Poly::new("alpha", "field", "default");
        assert_eq!(p.global_property("name").map(String::as_str), Some("alpha"));
        assert_eq!(p.global_property("type").map(String::as_str), Some("field"));
    }
}
