# zoneout

Rust port of the C++ [`zoneout`](https://codeberg.org/robolibs/zoneout) library
— hierarchical agricultural zones, workspaces, and geo-spatial plots for
robotics.

## Build

```sh
make build          # cargo build --lib --examples
make test           # cargo test --all-targets
make run EXAMPLE=quickstart
```

## Status

Work in progress — see [`PLAN.md`](./PLAN.md) for the conversion roadmap.

## Dependencies

Sibling robolibs Rust crates (pulled from Codeberg):

| crate     | role                                             |
|-----------|--------------------------------------------------|
| `datapod` | POD geometry (`Point`, `Geo`, `Polygon`, `Aabb`) |
| `concord` | coordinate transforms (WGS ↔ ENU)                |
| `graphix` | vertex graph for workspace nodes and edges       |
| `vectory` | GeoJSON feature collection I/O                   |
| `rastera` | multi-layer TIFF raster I/O                      |

## Bindings

```sh
make bind    # check C header and build Python wheel
make c-demo  # run the C ABI smoke demo
```
