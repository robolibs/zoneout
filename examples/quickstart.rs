//! Quickstart — build a farm / field hierarchy, drop a few waypoints,
//! connect them, save to a temp directory, reload, and print a summary.
//!
//! Ports `examples/quickstart.cpp` from the C++ zoneout library.

use std::collections::BTreeMap;

use datapod::{Geo, Point, Polygon};
use graphix::vertex::EdgeType;
use zoneout::{CoordMode, Workspace, ZoneBuilder};

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

fn main() -> zoneout::Result<()> {
    let datum = Geo::new(52.0, 5.0, 0.0);

    // Root zone: a farm with a large boundary.
    let mut farm = ZoneBuilder::new()
        .with_name("my_farm")
        .with_kind("farm")
        .with_boundary(square(200.0, (0.0, 0.0)))
        .with_datum(datum)
        .with_property("owner", "alice")
        .build()?;

    // Two field children inside the farm.
    let field_a = ZoneBuilder::new()
        .with_name("field_a")
        .with_kind("field")
        .with_boundary(square(30.0, (10.0, 10.0)))
        .with_datum(datum)
        .with_property("crop", "wheat")
        .build()?;

    let field_b = ZoneBuilder::new()
        .with_name("field_b")
        .with_kind("field")
        .with_boundary(square(30.0, (60.0, 60.0)))
        .with_datum(datum)
        .with_property("crop", "corn")
        .build()?;

    farm.add_child(field_a)?;
    farm.add_child(field_b)?;

    // Workspace = farm + empty graph.
    let mut ws = Workspace::new(farm);
    ws.set_datum(datum);
    ws.set_coord_mode(CoordMode::Local);

    // Drop three waypoints — two inside field_a, one outside.
    let a = ws.add_node(Point::new(15.0, 15.0, 0.0), BTreeMap::new());
    let b = ws.add_node(Point::new(25.0, 25.0, 0.0), BTreeMap::new());
    let c = ws.add_node(Point::new(70.0, 70.0, 0.0), BTreeMap::new());

    // Connect them.
    ws.add_edge(a, b, 10.0, EdgeType::Undirected, BTreeMap::new());
    ws.add_edge(b, c, 50.0, EdgeType::Directed, BTreeMap::new());

    println!("workspace: {ws:?}");
    println!("root: {} ({} children)", ws.root_zone().name(), ws.root_zone().child_count());
    for child in ws.root_zone().children() {
        println!(
            "  child: {} ({} nodes, crop={:?})",
            child.name(),
            child.node_ids().len(),
            child.property("crop"),
        );
    }

    // Point-in-zone query.
    let here = Point::new(15.0, 15.0, 0.0);
    let containing: Vec<&str> = ws.zones_containing(here).into_iter().map(|z| z.name()).collect();
    println!("zones containing {here:?}: {containing:?}");

    // Round-trip through disk.
    let mut tmp = std::env::temp_dir();
    tmp.push("zoneout-quickstart");
    let _ = std::fs::remove_dir_all(&tmp);
    ws.save(&tmp)?;
    let loaded = Workspace::load(&tmp)?;
    println!(
        "reloaded: root={}, children={}, coord_mode={:?}, nodes={}, edges={}",
        loaded.root_zone().name(),
        loaded.root_zone().child_count(),
        loaded.coord_mode(),
        loaded.graph().vertex_count(),
        loaded.graph().edge_count(),
    );

    let _ = std::fs::remove_dir_all(&tmp);
    Ok(())
}
