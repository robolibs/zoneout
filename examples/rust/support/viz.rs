//! Shared rerun helpers for zoneout examples. Mirrors the C++
//! `zoneout::visualize` namespace (`include/zoneout/visualize.hpp`). Keeps
//! the library itself rerun-free — rerun lives only in dev-dependencies.

#![allow(dead_code)]

use std::error::Error;

use concord::{Enu, to_wgs_from_enu};
use datapod::{Geo, Point};
use rerun::{Color, GeoLineStrings, LineStrips3D, RecordingStream, RecordingStreamBuilder};
use zoneout::Zone;

const PALETTE_RGB: [[u8; 3]; 10] = [
    [255, 100, 100], // Red
    [100, 255, 100], // Green
    [100, 100, 255], // Blue
    [255, 255, 100], // Yellow
    [100, 255, 255], // Cyan
    [255, 100, 255], // Magenta
    [255, 150, 100], // Orange
    [150, 100, 255], // Purple
    [100, 255, 150], // Light Green
    [255, 100, 150], // Pink
];

pub fn palette_color(index: usize) -> Color {
    let [r, g, b] = PALETTE_RGB[index % PALETTE_RGB.len()];
    Color::from_rgb(r, g, b)
}

/// Connect to a running rerun viewer (env `RERUN_URL` overrides the
/// default gRPC endpoint) or spawn a new process if the env var is unset
/// and `spawn=true`.
pub fn connect(app_id: &str) -> Result<RecordingStream, Box<dyn Error>> {
    if let Ok(url) = std::env::var("RERUN_URL") {
        let rec = RecordingStreamBuilder::new(app_id).connect_grpc_opts(url)?;
        return Ok(rec);
    }
    let rec = RecordingStreamBuilder::new(app_id).spawn()?;
    Ok(rec)
}

pub fn enu_to_latlon(p: Point, datum: Geo) -> [f64; 2] {
    let wgs = to_wgs_from_enu(Enu::new(p.x, p.y, p.z, datum));
    [wgs.latitude, wgs.longitude]
}

/// Draw a zone's boundary in both the 3D ENU view and the 2D Map view.
pub fn show_zone(
    rec: &RecordingStream,
    zone: &Zone,
    datum: Geo,
    zone_name: &str,
    color_index: usize,
) -> Result<(), Box<dyn Error>> {
    if !zone.poly().has_field_boundary() {
        eprintln!("Zone {zone_name} has no field boundary");
        return Ok(());
    }

    let color = palette_color(color_index);
    let boundary = zone.poly().field_boundary();
    if boundary.vertices.is_empty() {
        eprintln!("Zone {zone_name} has no points");
        return Ok(());
    }

    let enu_strip = close_3d(
        boundary
            .vertices
            .iter()
            .map(|v| [v.x as f32, v.y as f32, 0.0])
            .collect::<Vec<_>>(),
    );
    let wgs_strip = close_2d(
        boundary
            .vertices
            .iter()
            .map(|v| enu_to_latlon(*v, datum))
            .collect::<Vec<_>>(),
    );

    rec.log_static(
        format!("/{zone_name}/enu"),
        &LineStrips3D::new([enu_strip])
            .with_colors([color])
            .with_radii([0.5f32]),
    )?;
    rec.log_static(
        format!("/{zone_name}/wgs"),
        &GeoLineStrings::from_lat_lon([wgs_strip])
            .with_colors([color])
            .with_radii([2.0f32]),
    )?;

    Ok(())
}

/// ENU-only variant (no datum, no map view).
pub fn show_zone_enu(
    rec: &RecordingStream,
    zone: &Zone,
    zone_name: &str,
    color_index: usize,
) -> Result<(), Box<dyn Error>> {
    if !zone.poly().has_field_boundary() {
        eprintln!("Zone {zone_name} has no field boundary");
        return Ok(());
    }
    let color = palette_color(color_index);
    let strip = close_3d(
        zone.poly()
            .field_boundary()
            .vertices
            .iter()
            .map(|v| [v.x as f32, v.y as f32, 0.0])
            .collect::<Vec<_>>(),
    );
    rec.log_static(
        format!("/{zone_name}/enu"),
        &LineStrips3D::new([strip])
            .with_colors([color])
            .with_radii([0.5f32]),
    )?;
    Ok(())
}

/// Draw every polygon element inside a zone.
pub fn show_polygon_elements(
    rec: &RecordingStream,
    zone: &Zone,
    datum: Geo,
    zone_name: &str,
    height: f32,
) -> Result<(), Box<dyn Error>> {
    let element_color = Color::from_rgb(200, 200, 100);
    for (i, elem) in zone.poly().polygon_elements().iter().enumerate() {
        let enu_strip = close_3d(
            elem.geometry
                .vertices
                .iter()
                .map(|v| [v.x as f32, v.y as f32, height])
                .collect::<Vec<_>>(),
        );
        let wgs_strip = close_2d(
            elem.geometry
                .vertices
                .iter()
                .map(|v| enu_to_latlon(*v, datum))
                .collect::<Vec<_>>(),
        );
        let path = format!("/{zone_name}/elements/{}{i}", elem.meta.kind);

        rec.log_static(
            path.clone(),
            &LineStrips3D::new([enu_strip])
                .with_colors([element_color])
                .with_radii([0.3f32]),
        )?;
        rec.log_static(
            path,
            &GeoLineStrings::from_lat_lon([wgs_strip])
                .with_colors([element_color])
                .with_radii([1.5f32]),
        )?;
    }
    Ok(())
}

pub fn show_polygon_elements_enu(
    rec: &RecordingStream,
    zone: &Zone,
    zone_name: &str,
    height: f32,
) -> Result<(), Box<dyn Error>> {
    let element_color = Color::from_rgb(200, 200, 100);
    for (i, elem) in zone.poly().polygon_elements().iter().enumerate() {
        let strip = close_3d(
            elem.geometry
                .vertices
                .iter()
                .map(|v| [v.x as f32, v.y as f32, height])
                .collect::<Vec<_>>(),
        );
        let path = format!("/{zone_name}/elements/{}{i}", elem.meta.kind);
        rec.log_static(
            path,
            &LineStrips3D::new([strip])
                .with_colors([element_color])
                .with_radii([0.3f32]),
        )?;
    }
    Ok(())
}

fn close_3d(mut points: Vec<[f32; 3]>) -> Vec<[f32; 3]> {
    if let Some(first) = points.first().copied()
        && points.last().copied() != Some(first)
    {
        points.push(first);
    }
    points
}

fn close_2d(mut points: Vec<[f64; 2]>) -> Vec<[f64; 2]> {
    if let Some(first) = points.first().copied()
        && points.last().copied() != Some(first)
    {
        points.push(first);
    }
    points
}
