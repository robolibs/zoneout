# TODO

Comprehensive task list derived from feature gap analysis.

## 1. Persistence & Interoperability (High Priority)
- [ ] Implement Plot properties persistence (see `examples/GENPROP.md` guidance)
- [ ] Add Plot property injection in `Plot::save`
- [ ] Extract Plot properties in `Plot::load`
- [ ] Verify TAR save/load automatically preserves Plot metadata
- [ ] Add roundtrip tests for Plot properties (directory + TAR)
- [ ] Implement ISOXML conversion module (see `examples/GENISOXML.md`)
- [ ] Create `include/zoneout/zoneout/isoxml.hpp` and `src/isoxml.cpp`
- [ ] Add TinyXML2 dependency (FetchContent) to CMake
- [ ] Implement converter: boundary → PFD, polygons → TZN, grids → GRD, lines → GPN, points → PNT
- [ ] Add test for basic ISOXML export structure
- [ ] Improve GeoTIFF CRS metadata (EPSG:4326 support)
- [ ] Add GeoKeyDirectory full implementation (GeoAsciiParamsTag, GeoDoubleParamsTag)
- [ ] Add compression option (LZW) and tiled support (future)

## 2. Spatial Query & Analysis
- [ ] Add zone spatial query API: `findZonesWithin(point, radius)`
- [ ] Add polygon intersection & union utilities
- [ ] Add raster statistics API: min/max/mean per layer
- [ ] Add coverage calculation (% non-zero inside boundary)
- [ ] Implement time-series raster layer grouping (timestamp metadata)

## 3. Robot Navigation & Path Planning
- [ ] Add coverage path planning (boustrophedon algorithm)
- [ ] Add perimeter headland generation
- [ ] Add multi-robot task allocation primitives
- [ ] Add obstacle avoidance integration using occlusion layer
- [ ] Add API: `generatePath(start, end, mode)` with modes (direct, safe, coverage)

## 4. Data Validation & Quality Control
- [ ] Implement polygon geometry validation (self-intersection, degeneracy)
- [ ] Add raster layer validation (NoData detection, outliers)
- [ ] Add consistency check: all features inside zone boundary
- [ ] Add report generator: `zone.generateIntegrityReport()`
- [ ] Add tests for invalid geometries and boundary violations

## 5. Coordinate System & Transformations
- [ ] Add batch coordinate transformation utilities (WGS84 ↔ ENU ↔ UTM)
- [ ] Integrate EPSG lookup (basic subset)
- [ ] Add API: `plot.reproject(datum_or_epsg)` updating zones
- [ ] Store projection metadata in global properties

## 6. Feature API Expansion
- [ ] Document adding line and point features with examples
- [ ] Add `addLineFeature` and `addPointFeature` (if not present)
- [ ] Add filtering API for line/point by type/subtype
- [ ] Add visualization hooks for line/point overlay

## 7. Raster Layer Management Enhancements
- [ ] Add layer removal: `zone.removeRasterLayer(index)`
- [ ] Add layer insertion at index
- [ ] Add layer reorder API
- [ ] Add metadata update API: `zone.updateRasterLayerProps(index, props)`
- [ ] Support multi-band raster import/export
- [ ] Add resampling utilities (change resolution)

## 8. Serialization & Formats
- [ ] Add Shapefile export (optional dependency on GDAL)
- [ ] Add KML export (for Google Earth)
- [ ] Add GeoPackage export (future)
- [ ] Add CSV export for feature metadata
- [ ] Add lightweight JSON export (single file summary)
- [ ] Evaluate msgpack for compact binary serialization

## 9. Visualization Improvements
- [ ] Add web viewer (Leaflet/MapLibre JSON endpoint generator)
- [ ] Add legend/colormap configuration
- [ ] Add image export (PNG) for raster layers
- [ ] Add 3D terrain visualization (height map extrude)
- [ ] Add time slider demo for temporal layers

## 10. Performance & Optimization
- [ ] Expose spatial index (R-tree) for fast feature queries
- [ ] Add lazy loading for rasters (chunked reader)
- [ ] Add multithreaded layer generation (noise, sampling)
- [ ] Add caching layer for repeated spatial queries
- [ ] Add benchmark tests (zones, feature counts, raster sizes)

## 11. Testing Extensions
- [ ] Add integration test with synthetic farm (multiple zones + features + rasters + occlusion)
- [ ] Add performance benchmarks (timed doctest category)
- [ ] Add large raster stress test
- [ ] Add regression tests for file formats (golden files)
- [ ] Add ISOXML export validation test
- [ ] Add Plot properties backward compatibility test

## 12. Documentation Enhancements
- [ ] Generate Doxygen docs (add CMake option)
- [ ] Add architecture diagram (components: Poly/Grid/Layer/Plot/Zone)
- [ ] Add tutorial series (basic → advanced → robotics)
- [ ] Add troubleshooting guide (CRS issues, file I/O)
- [ ] Add performance tuning guide
- [ ] Add contributor guidelines for new spatial types

## 13. Examples Expansion
- [ ] Add multi-zone farm management example with queries
- [ ] Add ISOXML export example
- [ ] Add path planning example (simple coverage)
- [ ] Add time-series raster example
- [ ] Add sensor fusion example (combining layers)
- [ ] Add line/point feature usage example

## 14. Language Bindings
- [ ] Plan Python bindings (pybind11) — initial subset: Zone, Plot, basic queries
- [ ] Prepare CMake option `ZONEOUT_PYTHON_BINDINGS`
- [ ] Prototype conversion of Polygon + Grid to NumPy arrays
- [ ] Evaluate ROS2 message generation (future)

## 15. External Integration
- [ ] Add S3 storage adapter (save/load plot)
- [ ] Add weather API ingestion (layer generation)
- [ ] Add satellite imagery ingestion (placeholder pipeline)
- [ ] Add drone data import (GeoTIFF aligner)
- [ ] Add equipment telemetry parser stub

## 16. Cloud & Collaboration (Future)
- [ ] Design REST API layer (OpenAPI spec draft)
- [ ] Add change tracking (feature add/remove history)
- [ ] Add simple lock/claim mechanism for zones

## 17. Quality & Reliability
- [ ] Add integrity report generator for Plot
- [ ] Add UUID collision test (stress)
- [ ] Add deterministic mode for noise generation (seed control)

## 18. Backward Compatibility & Versioning
- [ ] Add `plot_version` global property
- [ ] Implement migration path (older file → current schema)
- [ ] Add version check warnings during load

## 19. Security & Robustness
- [ ] Validate all external file inputs (size, structure)
- [ ] Add safe temp directory handling for TAR extraction
- [ ] Add checksum manifest option for datasets

## 20. Nice-to-Have Enhancements
- [ ] Add plugin system for custom raster generation
- [ ] Add scripting hooks (embedded Lua/Python — long term)
- [ ] Add statistical zoning (auto-cluster raster into zones)

---

## Prioritization Snapshot
High: Plot persistence, ISOXML export, GeoTIFF CRS, Raster/Feature validation, Python bindings (plan)
Medium: Spatial queries, Path planning basics, Layer management extensions
Low: Cloud, advanced export formats, collaboration features

---

## Immediate Sprint Suggestion (First 2 Weeks)
1. Plot properties persistence implementation + tests
2. ISOXML converter initial version (boundary + one grid + one polygon feature)
3. GeoTIFF CRS metadata enhancement (EPSG tagging)
4. Raster statistics API (mean/min/max)
5. Documentation additions: Persistence + ISOXML HOWTO

---

Generated: 2025-11-14
