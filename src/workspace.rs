//! `Workspace` — a root `Zone` plus a `graphix::vertex::Graph` of waypoints
//! and navigation edges. Ported from `include/zoneout/zoneout/workspace.hpp`.

use std::collections::BTreeMap;
use std::fs;
use std::path::Path;

use datapod::{Geo, Point};
use graphix::vertex::{EdgeId, EdgeType, Graph, VertexId};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

use crate::core::error::{Error, Result};
use crate::wire::{
    self, CoordMode, EdgeJson, JsonGeo, JsonPoint, NodeJson, WorkspaceJson, ZoneJson,
};
use crate::zone::{Zone, ZoneBuilder};

const FILE_WORKSPACE_JSON: &str = "workspace.json";
const DIR_ZONES: &str = "zones";
const DIR_GRAPH: &str = "graph";
const FILE_GRAPH_JSON: &str = "graph.json";

// --- payload types --------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeData {
    pub id: Uuid,
    #[serde(default = "default_node_name")]
    pub name: String,
    pub position: NodePosition,
    #[serde(default)]
    pub zone_ids: Vec<Uuid>,
    #[serde(default)]
    pub properties: BTreeMap<String, String>,
}

fn default_node_name() -> String {
    "Node".into()
}

impl Default for NodeData {
    fn default() -> Self {
        Self {
            id: Uuid::new_v4(),
            name: default_node_name(),
            position: NodePosition::default(),
            zone_ids: Vec::new(),
            properties: BTreeMap::new(),
        }
    }
}

#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize, PartialEq)]
pub struct NodePosition {
    pub x: f64,
    pub y: f64,
    #[serde(default)]
    pub z: f64,
}

impl From<Point> for NodePosition {
    fn from(p: Point) -> Self {
        Self {
            x: p.x,
            y: p.y,
            z: p.z,
        }
    }
}

impl From<NodePosition> for Point {
    fn from(p: NodePosition) -> Self {
        Point::new(p.x, p.y, p.z)
    }
}

impl NodeData {
    pub fn new(position: Point) -> Self {
        Self {
            position: position.into(),
            ..Self::default()
        }
    }

    pub fn with_id(id: Uuid, position: Point) -> Self {
        Self {
            id,
            position: position.into(),
            ..Self::default()
        }
    }

    pub fn point(&self) -> Point {
        self.position.into()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeData {
    pub id: Uuid,
    #[serde(default)]
    pub zone_ids: Vec<Uuid>,
    #[serde(default)]
    pub properties: BTreeMap<String, String>,
}

impl Default for EdgeData {
    fn default() -> Self {
        Self {
            id: Uuid::new_v4(),
            zone_ids: Vec::new(),
            properties: BTreeMap::new(),
        }
    }
}

// --- workspace ------------------------------------------------------------

pub struct Workspace {
    root_zone: Zone,
    graph: Graph<NodeData, EdgeData>,
    datum: Option<Geo>,
    coord_mode: CoordMode,
}

impl std::fmt::Debug for Workspace {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Workspace")
            .field("root_zone", &self.root_zone.name())
            .field("vertices", &self.graph.vertex_count())
            .field("edges", &self.graph.edge_count())
            .field("coord_mode", &self.coord_mode)
            .finish()
    }
}

impl Workspace {
    pub fn new(root: Zone) -> Self {
        Self {
            root_zone: root,
            graph: Graph::new(),
            datum: None,
            coord_mode: CoordMode::Global,
        }
    }

    pub fn with_graph(root: Zone, graph: Graph<NodeData, EdgeData>) -> Self {
        Self {
            root_zone: root,
            graph,
            datum: None,
            coord_mode: CoordMode::Global,
        }
    }

    // -- identity & frame --------------------------------------------------

    pub fn root_zone(&self) -> &Zone {
        &self.root_zone
    }
    pub fn root_zone_mut(&mut self) -> &mut Zone {
        &mut self.root_zone
    }

    pub fn graph(&self) -> &Graph<NodeData, EdgeData> {
        &self.graph
    }
    pub fn graph_mut(&mut self) -> &mut Graph<NodeData, EdgeData> {
        &mut self.graph
    }

    pub fn datum(&self) -> Option<&Geo> {
        self.datum.as_ref()
    }
    pub fn set_datum(&mut self, datum: Geo) {
        self.datum = Some(datum);
    }
    pub fn clear_datum(&mut self) {
        self.datum = None;
    }
    pub fn has_datum(&self) -> bool {
        self.datum.is_some()
    }

    pub fn coord_mode(&self) -> CoordMode {
        self.coord_mode
    }
    pub fn set_coord_mode(&mut self, mode: CoordMode) {
        self.coord_mode = mode;
    }

    // -- zone queries ------------------------------------------------------

    pub fn find_zone(&self, zone_id: Uuid) -> Option<&Zone> {
        self.root_zone.find(zone_id)
    }
    pub fn find_zone_mut(&mut self, zone_id: Uuid) -> Option<&mut Zone> {
        self.root_zone.find_mut(zone_id)
    }

    /// Every zone in the tree whose boundary contains the point.
    pub fn zones_containing(&self, point: Point) -> Vec<&Zone> {
        fn walk<'a>(z: &'a Zone, p: Point, out: &mut Vec<&'a Zone>) {
            if z.contains(p) {
                out.push(z);
            }
            for c in z.children() {
                walk(c, p, out);
            }
        }
        let mut out = Vec::new();
        walk(&self.root_zone, point, &mut out);
        out
    }

    fn zone_ids_containing(&self, point: Point) -> Vec<Uuid> {
        self.zones_containing(point)
            .into_iter()
            .map(|z| z.id())
            .collect()
    }

    // -- graph mutation ----------------------------------------------------

    /// Add a waypoint. Computes zone membership from the current tree and
    /// attaches this node's id to each containing zone's `node_ids`.
    pub fn add_node(
        &mut self,
        position: Point,
        properties: BTreeMap<String, String>,
    ) -> VertexId<NodeData> {
        let zone_ids = self.zone_ids_containing(position);
        let mut node = NodeData::new(position);
        node.zone_ids = zone_ids.clone();
        node.properties = properties;
        let node_id = node.id;
        let vid = self.graph.add_vertex(node);
        self.attach_node_to_zones(node_id, &zone_ids);
        vid
    }

    pub fn add_node_data(&mut self, mut data: NodeData) -> VertexId<NodeData> {
        let zone_ids = self.zone_ids_containing(data.point());
        data.zone_ids = zone_ids.clone();
        let node_id = data.id;
        let vid = self.graph.add_vertex(data);
        self.attach_node_to_zones(node_id, &zone_ids);
        vid
    }

    pub fn add_edge(
        &mut self,
        source: VertexId<NodeData>,
        target: VertexId<NodeData>,
        weight: f64,
        edge_type: EdgeType,
        properties: BTreeMap<String, String>,
    ) -> EdgeId {
        let edge = EdgeData {
            id: Uuid::new_v4(),
            zone_ids: Vec::new(),
            properties,
        };
        self.graph.add_edge(source, target, weight, edge_type, edge)
    }

    pub fn add_edge_data(
        &mut self,
        source: VertexId<NodeData>,
        target: VertexId<NodeData>,
        weight: f64,
        edge_type: EdgeType,
        data: EdgeData,
    ) -> EdgeId {
        self.graph.add_edge(source, target, weight, edge_type, data)
    }

    fn attach_node_to_zones(&mut self, node_id: Uuid, zone_ids: &[Uuid]) {
        for zid in zone_ids {
            if let Some(z) = self.root_zone.find_mut(*zid) {
                if !z.node_ids().contains(&node_id) {
                    z.node_ids_mut().push(node_id);
                }
            }
        }
    }

    // -- find by Uuid ------------------------------------------------------

    /// Look up a vertex by its `NodeData.id`. Linear scan over vertices.
    pub fn find_node(&self, node_id: Uuid) -> Option<VertexId<NodeData>> {
        for vid in self.graph.vertices() {
            if let Some(n) = self.graph.get_vertex(vid) {
                if n.id == node_id {
                    return Some(vid);
                }
            }
        }
        None
    }

    /// Look up an edge by its `EdgeData.id`. Linear scan over vertex
    /// out-edges.
    pub fn find_edge(&self, edge_id: Uuid) -> Option<EdgeId> {
        for vid in self.graph.vertices() {
            for eid in self.graph.out_edges(vid) {
                if let Some(p) = self.graph.edge_property(eid) {
                    if p.id == edge_id {
                        return Some(eid);
                    }
                }
            }
        }
        None
    }

    // -- zone-membership refresh ------------------------------------------

    /// Recompute a single node's `zone_ids` from its current position.
    pub fn refresh_node_zone_membership(&mut self, vid: VertexId<NodeData>) {
        let Some(node) = self.graph.get_vertex(vid) else {
            return;
        };
        let new_ids = self.zone_ids_containing(node.point());
        if let Some(m) = self.graph.get_vertex_mut(vid) {
            m.zone_ids = new_ids;
        }
    }

    /// Recompute an edge's `zone_ids` as the **union** of its endpoints'
    /// zone memberships (mirrors C++ `refresh_edge_zone_membership`).
    pub fn refresh_edge_zone_membership(&mut self, eid: EdgeId) {
        let (Some(src), Some(tgt)) = (self.graph.source(eid), self.graph.target(eid)) else {
            return;
        };
        let src_zones = self
            .graph
            .get_vertex(src)
            .map(|n| n.zone_ids.clone())
            .unwrap_or_default();
        let tgt_zones = self
            .graph
            .get_vertex(tgt)
            .map(|n| n.zone_ids.clone())
            .unwrap_or_default();
        let mut merged: Vec<Uuid> = src_zones;
        for z in tgt_zones {
            if !merged.contains(&z) {
                merged.push(z);
            }
        }
        if let Some(edge) = self.graph.edge_property_mut(eid) {
            edge.zone_ids = merged;
        }
    }

    /// Full refresh: for every vertex, recompute its zone_ids; then rebuild
    /// zones' `node_ids`; then recompute edge zone memberships.
    pub fn refresh_graph_zone_membership(&mut self) {
        let vids: Vec<_> = self.graph.vertices();
        for vid in &vids {
            self.refresh_node_zone_membership(*vid);
        }
        self.refresh_zone_node_membership();
        let all_edges: Vec<EdgeId> = vids.iter().flat_map(|v| self.graph.out_edges(*v)).collect();
        for eid in all_edges {
            self.refresh_edge_zone_membership(eid);
        }
    }

    /// Clear every zone's `node_ids`, then repopulate by scanning graph
    /// vertices — mirrors C++ `refresh_zone_node_membership`.
    pub fn refresh_zone_node_membership(&mut self) {
        self.root_zone.visit_clear_node_ids();
        let vids = self.graph.vertices();
        for vid in vids {
            let Some(node) = self.graph.get_vertex(vid) else {
                continue;
            };
            let node_id = node.id;
            let zone_ids = node.zone_ids.clone();
            for zid in zone_ids {
                if let Some(z) = self.root_zone.find_mut(zid) {
                    if !z.node_ids().contains(&node_id) {
                        z.node_ids_mut().push(node_id);
                    }
                }
            }
        }
    }

    // -- persistence -------------------------------------------------------
    //
    // Disk layout:
    //   dir/workspace.json     — manifest (version, root_zone_id,
    //                            coord_mode, optional datum)
    //   dir/zones/             — recursive zone tree (child_0/, …)
    //   dir/graph/graph.json   — graph payload (nodes + edges)

    pub fn save(&self, directory: impl AsRef<Path>) -> Result<()> {
        let dir = directory.as_ref();
        fs::create_dir_all(dir).map_err(|e| Error::io(dir, e))?;

        self.root_zone.save(dir.join(DIR_ZONES))?;

        let manifest = Manifest {
            format_version: 2,
            root_zone_id: self.root_zone.id(),
            coord_mode: self.coord_mode,
            datum: self.datum.map(JsonGeo::from),
        };
        let manifest_path = dir.join(FILE_WORKSPACE_JSON);
        let json = serde_json::to_vec_pretty(&manifest)?;
        fs::write(&manifest_path, json).map_err(|e| Error::io(&manifest_path, e))?;

        // Graph payload: nodes + edges, snapshotted via graphix accessors.
        let graph_dir = dir.join(DIR_GRAPH);
        fs::create_dir_all(&graph_dir).map_err(|e| Error::io(&graph_dir, e))?;
        let graph_json = self.graph_to_json();
        let graph_path = graph_dir.join(FILE_GRAPH_JSON);
        let bytes = serde_json::to_vec_pretty(&graph_json)?;
        fs::write(&graph_path, bytes).map_err(|e| Error::io(&graph_path, e))?;

        Ok(())
    }

    pub fn load(directory: impl AsRef<Path>) -> Result<Self> {
        let dir = directory.as_ref();

        let manifest_path = dir.join(FILE_WORKSPACE_JSON);
        if !manifest_path.exists() {
            return Err(Error::NotFound(format!(
                "workspace.json not found in {}",
                dir.display()
            )));
        }
        let bytes = fs::read(&manifest_path).map_err(|e| Error::io(&manifest_path, e))?;
        let manifest: Manifest = serde_json::from_slice(&bytes)?;

        let zones_dir = dir.join(DIR_ZONES);
        if !zones_dir.is_dir() {
            return Err(Error::NotFound(format!(
                "no zones/ directory in {}",
                dir.display()
            )));
        }
        let root = Zone::load(zones_dir)?;

        let mut ws = Self::new(root);
        ws.coord_mode = manifest.coord_mode;
        ws.datum = manifest.datum.map(Geo::from);

        let graph_path = dir.join(DIR_GRAPH).join(FILE_GRAPH_JSON);
        if graph_path.exists() {
            let bytes = fs::read(&graph_path).map_err(|e| Error::io(&graph_path, e))?;
            let snap: GraphJson = serde_json::from_slice(&bytes)?;
            ws.hydrate_graph(snap);
            ws.refresh_zone_node_membership();
        }

        Ok(ws)
    }

    fn graph_to_json(&self) -> GraphJson {
        let vertices = self.graph.vertices();
        let mut vertex_index: std::collections::HashMap<Uuid, usize> =
            std::collections::HashMap::with_capacity(vertices.len());
        let mut nodes = Vec::with_capacity(vertices.len());
        for (i, vid) in vertices.iter().enumerate() {
            if let Some(n) = self.graph.get_vertex(*vid) {
                vertex_index.insert(n.id, i);
                nodes.push(GraphNodeJson {
                    id: n.id,
                    name: n.name.clone(),
                    position: n.position,
                    zone_ids: n.zone_ids.clone(),
                    properties: n.properties.clone(),
                });
            }
        }

        let mut edges = Vec::new();
        let mut seen_edge_ids: std::collections::HashSet<Uuid> = std::collections::HashSet::new();
        for vid in &vertices {
            for eid in self.graph.out_edges(*vid) {
                let Some(prop) = self.graph.edge_property(eid) else {
                    continue;
                };
                if !seen_edge_ids.insert(prop.id) {
                    continue;
                }
                let Some(src) = self.graph.source(eid) else {
                    continue;
                };
                let Some(tgt) = self.graph.target(eid) else {
                    continue;
                };
                let Some(src_node) = self.graph.get_vertex(src) else {
                    continue;
                };
                let Some(tgt_node) = self.graph.get_vertex(tgt) else {
                    continue;
                };
                let Some(et) = self.graph.get_edge_type(eid) else {
                    continue;
                };
                let Some(w) = self.graph.get_weight(eid) else {
                    continue;
                };
                edges.push(GraphEdgeJson {
                    id: prop.id,
                    source_id: src_node.id,
                    target_id: tgt_node.id,
                    directed: matches!(et, EdgeType::Directed),
                    weight: w,
                    zone_ids: prop.zone_ids.clone(),
                    properties: prop.properties.clone(),
                });
            }
        }
        GraphJson { nodes, edges }
    }

    // -- wire conversion ---------------------------------------------------

    /// Build a workspace from a flat `WorkspaceJson` draft (the format the
    /// C++ library reads/writes as a single JSON file). Mirrors C++
    /// `zoneout::to_workspace`.
    pub fn from_wire(ws_json: WorkspaceJson) -> Result<Self> {
        wire::require_valid_workspace_json(&ws_json).map_err(Error::InvalidZone)?;

        let datum = wire::infer_datum(&ws_json).map_err(Error::InvalidZone)?;
        let root_id = wire::infer_root_zone_id(&ws_json).map_err(Error::InvalidZone)?;

        // Group children by parent id.
        let mut children_by_parent: std::collections::HashMap<Uuid, Vec<Uuid>> =
            std::collections::HashMap::new();
        for (id, z) in &ws_json.zones {
            if let Some(pid) = z.parent_id {
                children_by_parent.entry(pid).or_default().push(*id);
            }
        }

        let mut visiting: std::collections::HashSet<Uuid> = std::collections::HashSet::new();
        let mut built: std::collections::HashSet<Uuid> = std::collections::HashSet::new();
        let root = build_zone_tree(
            &ws_json,
            root_id,
            datum,
            &children_by_parent,
            &mut visiting,
            &mut built,
        )?;

        if built.len() != ws_json.zones.len() {
            for id in ws_json.zones.keys() {
                if !built.contains(id) {
                    return Err(Error::InvalidZone(format!(
                        "Zone '{id}' is disconnected from the root zone"
                    )));
                }
            }
        }

        let mut ws = Self::new(root);
        ws.coord_mode = ws_json.coord_mode;
        ws.datum = ws_json
            .ref_
            .map(|p| Geo::new(p.lat, p.lon, 0.0))
            .or_else(|| ws_json.datum.map(Geo::from));

        // Nodes
        let mut node_vid_by_node_id: std::collections::HashMap<Uuid, VertexId<NodeData>> =
            std::collections::HashMap::with_capacity(ws_json.nodes.len());
        for (_wire_id, draft) in &ws_json.nodes {
            let local = wire::to_local_point(draft.latlon, datum, ws_json.coord_mode);
            let mut props = draft.properties.clone();
            let name = props.remove("name").unwrap_or_else(|| draft.name.clone());
            let mut node = NodeData {
                id: draft.id,
                name,
                position: local.into(),
                zone_ids: Vec::new(),
                properties: props,
            };
            // computed membership (spatial)
            let computed: Vec<Uuid> = {
                let mut out = Vec::new();
                walk_zones(&ws.root_zone, local, &mut out);
                out
            };
            node.zone_ids.extend(computed);
            for id in &draft.zone_ids {
                if !node.zone_ids.contains(id) {
                    node.zone_ids.push(*id);
                }
            }
            let vid = ws.graph.add_vertex(node);
            node_vid_by_node_id.insert(draft.id, vid);
        }

        // Edges
        for (_wire_id, draft) in &ws_json.edges {
            let (Some(&s), Some(&t)) = (
                node_vid_by_node_id.get(&draft.source_id),
                node_vid_by_node_id.get(&draft.target_id),
            ) else {
                return Err(Error::InvalidZone(format!(
                    "Edge '{}' references unknown node ids",
                    draft.id
                )));
            };
            let et = if draft.directed {
                EdgeType::Directed
            } else {
                EdgeType::Undirected
            };
            let edge = EdgeData {
                id: draft.id,
                zone_ids: Vec::new(),
                properties: draft.properties.clone(),
            };
            let eid = ws.graph.add_edge(s, t, draft.weight, et, edge);

            // Merge zone_ids from endpoints + manual list
            let s_zones = ws
                .graph
                .get_vertex(s)
                .map(|n| n.zone_ids.clone())
                .unwrap_or_default();
            let t_zones = ws
                .graph
                .get_vertex(t)
                .map(|n| n.zone_ids.clone())
                .unwrap_or_default();
            let mut merged: Vec<Uuid> = Vec::new();
            for z in s_zones
                .into_iter()
                .chain(t_zones)
                .chain(draft.zone_ids.iter().copied())
            {
                if !merged.contains(&z) {
                    merged.push(z);
                }
            }
            if let Some(ep) = ws.graph.edge_property_mut(eid) {
                ep.zone_ids = merged;
            }
        }

        // Zone node_ids: accept imported set if non-empty; otherwise
        // recompute from graph.
        ws.refresh_zone_node_membership();
        for (id, draft) in &ws_json.zones {
            if !draft.node_ids.is_empty() {
                if let Some(z) = ws.root_zone.find_mut(*id) {
                    z.set_node_ids(draft.node_ids.clone());
                }
            }
        }
        Ok(ws)
    }

    /// Flatten a `Workspace` to a `WorkspaceJson` draft. Mirrors C++
    /// `zoneout::from_workspace`.
    pub fn to_wire(&self) -> WorkspaceJson {
        let datum = self.datum.unwrap_or_else(|| *self.root_zone.plot().datum());
        let mut ws_json = WorkspaceJson {
            root_zone_id: self.root_zone.id(),
            coord_mode: self.coord_mode,
            ref_: self.datum.map(|d| JsonPoint::new(d.latitude, d.longitude)),
            datum: Some(JsonGeo::from(datum)),
            zones: BTreeMap::new(),
            nodes: BTreeMap::new(),
            edges: BTreeMap::new(),
            name: "Workspace".to_string(),
        };
        append_zone_to_wire(&mut ws_json, &self.root_zone, None, datum);

        // Nodes
        for vid in self.graph.vertices() {
            let Some(n) = self.graph.get_vertex(vid) else {
                continue;
            };
            let mut props = n.properties.clone();
            props.entry("name".into()).or_insert_with(|| n.name.clone());
            let latlon = wire::to_json_point(n.point(), datum, self.coord_mode);
            ws_json.nodes.insert(
                n.id,
                NodeJson {
                    id: n.id,
                    name: n.name.clone(),
                    latlon,
                    zone_ids: n.zone_ids.clone(),
                    properties: props,
                },
            );
        }

        // Edges
        let mut seen_edges: std::collections::HashSet<Uuid> = std::collections::HashSet::new();
        for vid in self.graph.vertices() {
            for eid in self.graph.out_edges(vid) {
                let Some(prop) = self.graph.edge_property(eid) else {
                    continue;
                };
                if !seen_edges.insert(prop.id) {
                    continue;
                }
                let (Some(src), Some(tgt)) = (self.graph.source(eid), self.graph.target(eid))
                else {
                    continue;
                };
                let Some(src_n) = self.graph.get_vertex(src) else {
                    continue;
                };
                let Some(tgt_n) = self.graph.get_vertex(tgt) else {
                    continue;
                };
                let Some(et) = self.graph.get_edge_type(eid) else {
                    continue;
                };
                let Some(w) = self.graph.get_weight(eid) else {
                    continue;
                };
                ws_json.edges.insert(
                    prop.id,
                    EdgeJson {
                        id: prop.id,
                        source_id: src_n.id,
                        target_id: tgt_n.id,
                        directed: matches!(et, EdgeType::Directed),
                        weight: w,
                        zone_ids: prop.zone_ids.clone(),
                        properties: prop.properties.clone(),
                    },
                );
            }
        }

        // Sync zone node_ids (authoritative on the Zone tree)
        for (id, z) in ws_json.zones.iter_mut() {
            if let Some(zone) = self.root_zone.find(*id) {
                z.node_ids = zone.node_ids().to_vec();
            }
        }

        ws_json
    }

    fn hydrate_graph(&mut self, snap: GraphJson) {
        // Clear any pre-existing graph state so the wire payload is the
        // single source of truth after load.
        self.graph = Graph::new();

        let mut by_node_id: std::collections::HashMap<Uuid, VertexId<NodeData>> =
            std::collections::HashMap::with_capacity(snap.nodes.len());
        for n in snap.nodes {
            let data = NodeData {
                id: n.id,
                name: n.name,
                position: n.position,
                zone_ids: n.zone_ids,
                properties: n.properties,
            };
            let id = data.id;
            let vid = self.graph.add_vertex(data);
            by_node_id.insert(id, vid);
        }
        for e in snap.edges {
            let (Some(&s), Some(&t)) = (by_node_id.get(&e.source_id), by_node_id.get(&e.target_id))
            else {
                continue;
            };
            let ed = EdgeData {
                id: e.id,
                zone_ids: e.zone_ids,
                properties: e.properties,
            };
            let et = if e.directed {
                EdgeType::Directed
            } else {
                EdgeType::Undirected
            };
            self.graph.add_edge(s, t, e.weight, et, ed);
        }
    }
}

// --- wire conversion helpers ---------------------------------------------

fn walk_zones(z: &Zone, p: Point, out: &mut Vec<Uuid>) {
    if z.contains(p) {
        out.push(z.id());
    }
    for c in z.children() {
        walk_zones(c, p, out);
    }
}

fn build_zone_tree(
    ws: &WorkspaceJson,
    id: Uuid,
    datum: Geo,
    children_by_parent: &std::collections::HashMap<Uuid, Vec<Uuid>>,
    visiting: &mut std::collections::HashSet<Uuid>,
    built: &mut std::collections::HashSet<Uuid>,
) -> Result<Zone> {
    let draft = ws
        .zones
        .get(&id)
        .ok_or_else(|| Error::InvalidZone(format!("Zone id '{id}' not found in draft")))?;
    if visiting.contains(&id) {
        return Err(Error::InvalidZone(format!(
            "Zone hierarchy contains a cycle at zone '{id}'"
        )));
    }
    if built.contains(&id) {
        return Err(Error::InvalidZone(format!(
            "Zone '{id}' appears more than once in the hierarchy"
        )));
    }
    if draft.polygon_latlon.len() < 3 {
        return Err(Error::InvalidZone(format!(
            "Zone '{id}' must have at least 3 polygon vertices"
        )));
    }
    visiting.insert(id);

    let boundary = wire::to_local_polygon(&draft.polygon_latlon, datum, ws.coord_mode);
    let resolution = if draft.grid_enabled && draft.grid_resolution > 0.0 {
        draft.grid_resolution
    } else {
        0.0
    };
    let mut zone = ZoneBuilder::new()
        .with_name(&draft.name)
        .with_kind(&draft.kind)
        .with_boundary(boundary)
        .with_datum(datum)
        .with_resolution(resolution)
        .build()?;
    zone.set_id(draft.id);
    for (k, v) in &draft.properties {
        zone.set_property(k, v);
    }

    if let Some(children) = children_by_parent.get(&id) {
        for cid in children {
            let child = build_zone_tree(ws, *cid, datum, children_by_parent, visiting, built)?;
            zone.add_child(child)?;
        }
    }

    visiting.remove(&id);
    built.insert(id);
    Ok(zone)
}

fn append_zone_to_wire(
    ws_json: &mut WorkspaceJson,
    zone: &Zone,
    parent_id: Option<Uuid>,
    datum: Geo,
) {
    let plot = zone.plot();
    let grid_enabled = plot.has_grid();
    let grid_resolution = if grid_enabled {
        plot.grid().map(|g| g.resolution()).unwrap_or(1.0)
    } else {
        1.0
    };
    let polygon_latlon =
        wire::to_json_polygon(plot.poly().field_boundary(), datum, ws_json.coord_mode);
    let child_ids: Vec<Uuid> = zone.children().iter().map(|c| c.id()).collect();

    ws_json.zones.insert(
        zone.id(),
        ZoneJson {
            id: zone.id(),
            name: zone.name().to_string(),
            kind: zone.kind().to_string(),
            parent_id,
            child_ids,
            node_ids: zone.node_ids().to_vec(),
            properties: zone.properties().clone(),
            polygon_latlon,
            grid_enabled,
            grid_resolution,
        },
    );

    for c in zone.children() {
        append_zone_to_wire(ws_json, c, Some(zone.id()), datum);
    }
}

// --- manifest -------------------------------------------------------------

#[derive(Debug, Serialize, Deserialize)]
struct Manifest {
    format_version: u32,
    root_zone_id: Uuid,
    coord_mode: CoordMode,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    datum: Option<JsonGeo>,
}

// --- graph wire payload ---------------------------------------------------

#[derive(Debug, Default, Serialize, Deserialize)]
struct GraphJson {
    #[serde(default)]
    nodes: Vec<GraphNodeJson>,
    #[serde(default)]
    edges: Vec<GraphEdgeJson>,
}

#[derive(Debug, Serialize, Deserialize)]
struct GraphNodeJson {
    id: Uuid,
    #[serde(default = "default_node_name")]
    name: String,
    position: NodePosition,
    #[serde(default)]
    zone_ids: Vec<Uuid>,
    #[serde(default)]
    properties: BTreeMap<String, String>,
}

#[derive(Debug, Serialize, Deserialize)]
struct GraphEdgeJson {
    id: Uuid,
    source_id: Uuid,
    target_id: Uuid,
    #[serde(default)]
    directed: bool,
    #[serde(default = "default_edge_weight")]
    weight: f64,
    #[serde(default)]
    zone_ids: Vec<Uuid>,
    #[serde(default)]
    properties: BTreeMap<String, String>,
}

fn default_edge_weight() -> f64 {
    1.0
}

// --- tests ----------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use datapod::Polygon;

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

    fn farm() -> Zone {
        crate::zone::ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(100.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .build()
            .unwrap()
    }

    #[test]
    fn ws_zones_containing_root_only() {
        let ws = Workspace::new(farm());
        assert_eq!(ws.zones_containing(Point::new(10.0, 10.0, 0.0)).len(), 1);
        assert_eq!(ws.zones_containing(Point::new(-1.0, 0.0, 0.0)).len(), 0);
    }

    #[test]
    fn ws_add_node_attaches_to_zone() {
        let mut ws = Workspace::new(farm());
        let root_id = ws.root_zone.id();
        ws.add_node(Point::new(5.0, 5.0, 0.0), BTreeMap::new());
        let z = ws.find_zone(root_id).unwrap();
        assert_eq!(z.node_ids().len(), 1);
    }

    #[test]
    fn ws_add_edge_counts() {
        let mut ws = Workspace::new(farm());
        let a = ws.add_node(Point::new(1.0, 1.0, 0.0), BTreeMap::new());
        let b = ws.add_node(Point::new(2.0, 2.0, 0.0), BTreeMap::new());
        ws.add_edge(a, b, 1.0, EdgeType::Undirected, BTreeMap::new());
        assert_eq!(ws.graph().vertex_count(), 2);
        assert_eq!(ws.graph().edge_count(), 1);
    }

    #[test]
    fn ws_find_node_and_edge_by_uuid() {
        let mut ws = Workspace::new(farm());
        let a = ws.add_node(Point::new(1.0, 1.0, 0.0), BTreeMap::new());
        let node_uuid = ws.graph().get_vertex(a).unwrap().id;
        let eid = ws.add_edge(a, a, 1.0, EdgeType::Directed, BTreeMap::new());
        let edge_uuid = ws.graph().edge_property(eid).unwrap().id;

        assert_eq!(ws.find_node(node_uuid), Some(a));
        assert!(ws.find_node(Uuid::new_v4()).is_none());
        assert_eq!(ws.find_edge(edge_uuid), Some(eid));
        assert!(ws.find_edge(Uuid::new_v4()).is_none());
    }

    #[test]
    fn ws_refresh_zone_node_membership_repopulates() {
        let mut ws = Workspace::new(farm());
        let root_id = ws.root_zone.id();
        ws.add_node(Point::new(5.0, 5.0, 0.0), BTreeMap::new());
        ws.add_node(Point::new(10.0, 10.0, 0.0), BTreeMap::new());
        ws.find_zone_mut(root_id).unwrap().clear_node_ids();
        assert_eq!(ws.find_zone(root_id).unwrap().node_ids().len(), 0);
        ws.refresh_zone_node_membership();
        assert_eq!(ws.find_zone(root_id).unwrap().node_ids().len(), 2);
    }

    #[test]
    fn ws_save_and_load_manifest() {
        let mut ws = Workspace::new(farm());
        ws.set_datum(Geo::new(52.0, 5.0, 0.0));
        ws.set_coord_mode(CoordMode::Local);

        let mut dir = std::env::temp_dir();
        dir.push(format!("zoneout-ws-{}", Uuid::new_v4().simple()));
        ws.save(&dir).expect("save");

        let back = Workspace::load(&dir).expect("load");
        assert_eq!(back.coord_mode(), CoordMode::Local);
        assert!(back.has_datum());
        assert_eq!(back.root_zone().name(), "farm");

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn ws_save_and_load_multi_zone_with_geojson_and_tiff() {
        // Build: farm (with grid) → field_a (with grid) + field_b (with grid)
        let datum = Geo::new(52.0, 5.0, 0.0);
        let mut farm = crate::zone::ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(200.0, (0.0, 0.0)))
            .with_datum(datum)
            .with_resolution(1.0)
            .build()
            .unwrap();
        let field_a = crate::zone::ZoneBuilder::new()
            .with_name("field_a")
            .with_kind("field")
            .with_boundary(square(30.0, (10.0, 10.0)))
            .with_datum(datum)
            .with_resolution(1.0)
            .build()
            .unwrap();
        let field_b = crate::zone::ZoneBuilder::new()
            .with_name("field_b")
            .with_kind("field")
            .with_boundary(square(30.0, (60.0, 60.0)))
            .with_datum(datum)
            .with_resolution(1.0)
            .build()
            .unwrap();
        let field_a_id = field_a.id();
        farm.add_child(field_a).unwrap();
        farm.add_child(field_b).unwrap();

        let mut ws = Workspace::new(farm);
        ws.set_datum(datum);
        ws.add_node(Point::new(15.0, 15.0, 0.0), BTreeMap::new());
        ws.add_node(Point::new(70.0, 70.0, 0.0), BTreeMap::new());

        // Save to a tmpdir.
        let mut dir = std::env::temp_dir();
        dir.push(format!("zoneout-multi-{}", Uuid::new_v4().simple()));
        ws.save(&dir).expect("save");

        // Verify: every zone produces its own vector.geojson + raster.tiff.
        let root = dir.join("zones");
        assert!(
            root.join("vector.geojson").is_file(),
            "root vector.geojson missing"
        );
        assert!(
            root.join("raster.tiff").is_file(),
            "root raster.tiff missing"
        );
        assert!(root.join("zone.json").is_file(), "root zone.json missing");

        for i in 0..2 {
            let c = root.join(format!("child_{i}"));
            assert!(
                c.join("vector.geojson").is_file(),
                "child {i} vector.geojson missing"
            );
            assert!(
                c.join("raster.tiff").is_file(),
                "child {i} raster.tiff missing"
            );
            assert!(c.join("zone.json").is_file(), "child {i} zone.json missing");
        }

        assert!(dir.join("workspace.json").is_file());
        assert!(dir.join("graph").join("graph.json").is_file());

        // Load it back and verify the whole tree is intact.
        let back = Workspace::load(&dir).expect("load");
        assert_eq!(back.root_zone().name(), "farm");
        assert_eq!(back.root_zone().child_count(), 2);
        assert_eq!(back.find_zone(field_a_id).unwrap().name(), "field_a");
        assert!(back.root_zone().has_grid());
        assert!(back.root_zone().children()[0].has_grid());
        assert!(back.root_zone().children()[1].has_grid());
        assert_eq!(back.graph().vertex_count(), 2);

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn ws_save_and_load_full_graph_roundtrip() {
        let mut ws = Workspace::new(farm());
        let a = ws.add_node(Point::new(5.0, 5.0, 0.0), BTreeMap::new());
        let b = ws.add_node(Point::new(50.0, 50.0, 0.0), BTreeMap::new());
        let c = ws.add_node(Point::new(10.0, 10.0, 0.0), BTreeMap::new());
        ws.add_edge(a, b, 2.5, EdgeType::Undirected, BTreeMap::new());
        ws.add_edge(b, c, 7.5, EdgeType::Directed, {
            let mut p = BTreeMap::new();
            p.insert("kind".into(), "path".into());
            p
        });
        let expected_node_count = ws.graph().vertex_count();
        let expected_edge_count = ws.graph().edge_count();
        let root_id = ws.root_zone().id();

        let mut dir = std::env::temp_dir();
        dir.push(format!("zoneout-ws-gfull-{}", Uuid::new_v4().simple()));
        ws.save(&dir).expect("save");

        let back = Workspace::load(&dir).expect("load");
        assert_eq!(back.graph().vertex_count(), expected_node_count);
        assert_eq!(back.graph().edge_count(), expected_edge_count);
        // zone node_ids should have been re-attached on load
        assert_eq!(back.find_zone(root_id).unwrap().node_ids().len(), 3);

        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn ws_wire_roundtrip_preserves_structure() {
        let mut ws = Workspace::new(farm());
        ws.set_datum(Geo::new(52.0, 5.0, 0.0));
        let a = ws.add_node(Point::new(5.0, 5.0, 0.0), BTreeMap::new());
        let b = ws.add_node(Point::new(20.0, 20.0, 0.0), BTreeMap::new());
        ws.add_edge(a, b, 3.0, EdgeType::Undirected, BTreeMap::new());

        let wire_json = ws.to_wire();
        let errs = wire::validate_workspace_json(&wire_json);
        assert!(errs.is_empty(), "validation errors: {errs:?}");

        let back = Workspace::from_wire(wire_json).expect("from_wire");
        assert_eq!(back.root_zone().name(), "farm");
        assert_eq!(back.graph().vertex_count(), 2);
        assert_eq!(back.graph().edge_count(), 1);
    }

    #[test]
    fn ws_wire_roundtrip_with_children() {
        let mut parent = crate::zone::ZoneBuilder::new()
            .with_name("farm")
            .with_kind("farm")
            .with_boundary(square(200.0, (0.0, 0.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_resolution(0.0)
            .build()
            .unwrap();
        let child = crate::zone::ZoneBuilder::new()
            .with_name("field_a")
            .with_kind("field")
            .with_boundary(square(30.0, (5.0, 5.0)))
            .with_datum(Geo::new(52.0, 5.0, 0.0))
            .with_resolution(0.0)
            .build()
            .unwrap();
        let child_id = child.id();
        parent.add_child(child).unwrap();

        let mut ws = Workspace::new(parent);
        ws.set_datum(Geo::new(52.0, 5.0, 0.0));

        let wire_json = ws.to_wire();
        assert!(wire::validate_workspace_json(&wire_json).is_empty());

        let back = Workspace::from_wire(wire_json).unwrap();
        assert_eq!(back.root_zone().child_count(), 1);
        assert_eq!(back.find_zone(child_id).unwrap().name(), "field_a");
    }

    #[test]
    fn ws_single_file_json_roundtrip() {
        let mut ws = Workspace::new(farm());
        ws.set_datum(Geo::new(52.0, 5.0, 0.0));
        ws.add_node(Point::new(5.0, 5.0, 0.0), BTreeMap::new());

        let mut path = std::env::temp_dir();
        path.push(format!("zoneout-wire-{}.json", Uuid::new_v4().simple()));
        crate::io::write_workspace_json_file(&path, &ws).expect("write");
        let back = crate::io::load_workspace_json_file(&path).expect("load");
        assert_eq!(back.root_zone().name(), "farm");
        assert_eq!(back.graph().vertex_count(), 1);
        let _ = fs::remove_file(&path);
    }

    #[test]
    fn ws_validation_catches_missing_vertices() {
        use crate::wire::{WorkspaceJson, ZoneJson};
        let mut ws = WorkspaceJson::default();
        let zid = Uuid::new_v4();
        ws.root_zone_id = zid;
        ws.coord_mode = CoordMode::Global;
        let mut zone = ZoneJson::default();
        zone.id = zid;
        // only 2 vertices (needs >= 3)
        zone.polygon_latlon = vec![
            crate::wire::JsonPoint::new(52.0, 5.0),
            crate::wire::JsonPoint::new(52.1, 5.0),
        ];
        ws.zones.insert(zid, zone);
        let errs = wire::validate_workspace_json(&ws);
        assert!(
            errs.iter()
                .any(|e| e.contains("at least 3 polygon vertices"))
        );
    }

    #[test]
    fn ws_refresh_edge_zone_membership_writes_back() {
        let mut ws = Workspace::new(farm());
        let root_id = ws.root_zone().id();
        let a = ws.add_node(Point::new(5.0, 5.0, 0.0), BTreeMap::new());
        let b = ws.add_node(Point::new(10.0, 10.0, 0.0), BTreeMap::new());
        let e = ws.add_edge(a, b, 1.0, EdgeType::Undirected, BTreeMap::new());
        ws.refresh_edge_zone_membership(e);
        let zones = &ws.graph().edge_property(e).unwrap().zone_ids;
        assert!(zones.contains(&root_id));
    }
}
