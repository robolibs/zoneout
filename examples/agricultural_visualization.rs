//! Agricultural visualization — builds a farm with two fields and a few
//! polygon obstacles, then logs everything to rerun in both ENU (3D) and
//! WGS (Map) views.
//!
//! Run a rerun viewer first (`rerun`) or set `RERUN_URL=...`. Ports the
//! C++ `examples/agricultural_visualization_example.cpp`.

#[path = "support/viz.rs"]
mod viz;

use datapod::{Geo, Point, Polygon};
use zoneout::ZoneBuilder;

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

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let datum = Geo::new(52.2741, 5.6459, 0.0);

    let mut farm = ZoneBuilder::new()
        .with_name("farm")
        .with_kind("farm")
        .with_boundary(square(200.0, (0.0, 0.0)))
        .with_datum(datum)
        .build()?;

    let field_a = ZoneBuilder::new()
        .with_name("field_a")
        .with_kind("field")
        .with_boundary(square(50.0, (10.0, 10.0)))
        .with_datum(datum)
        .build()?;

    let field_b = ZoneBuilder::new()
        .with_name("field_b")
        .with_kind("field")
        .with_boundary(square(60.0, (90.0, 90.0)))
        .with_datum(datum)
        .build()?;

    let field_a_id = field_a.id();
    let field_b_id = field_b.id();
    farm.add_child(field_a)?;
    farm.add_child(field_b)?;

    // attach a small obstacle polygon inside field_a
    let obstacle = square(5.0, (20.0, 20.0));
    if let Some(fa) = farm.find_mut(field_a_id) {
        fa.add_polygon_element(
            obstacle,
            "rock",
            "obstacle",
            "default",
            Default::default(),
        )?;
    }

    // connect to rerun
    let rec = viz::connect("zoneout_agri_viz")?;

    viz::show_zone(&rec, &farm, datum, "farm", 0)?;
    for (i, child) in farm.children().iter().enumerate() {
        viz::show_zone(&rec, child, datum, child.name(), i + 1)?;
        viz::show_polygon_elements(&rec, child, datum, child.name(), 0.1)?;
    }

    let _ = field_b_id; // kept for parity with C++ example

    println!("logged farm + {} fields to rerun", farm.child_count());
    Ok(())
}
