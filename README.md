<img align="right" width="26%" src="./misc/logo.png">

# Zoneout

**C++ Library for Agricultural Robotics Workspace Management**

Combines vector geometry (boundaries, features) with raster data (elevation, moisture) for autonomous farming coordination.

## Quick Start

```bash
git clone https://github.com/your-org/zoneout && cd zoneout
mkdir build && cd build
cmake -DZONEOUT_BUILD_EXAMPLES=ON -DZONEOUT_ENABLE_TESTS=ON ..
make && make test
./quickstart  # Run 5-minute tutorial
```

### Minimal Example

```cpp
#include "zoneout/zoneout.hpp"

int main() {
    // 1. Create boundary (100m × 50m field)
    concord::Polygon boundary;
    boundary.addPoint(concord::Point{0, 0, 0});
    boundary.addPoint(concord::Point{100, 0, 0});
    boundary.addPoint(concord::Point{100, 50, 0});
    boundary.addPoint(concord::Point{0, 50, 0});
    
    // 2. Create zone with GPS datum and 1m resolution
    concord::Datum datum{52.0, 5.0, 0.0};  // lat, lon, alt
    zoneout::Zone zone("field", "agricultural", boundary, datum, 1.0);
    
    // 3. Add properties and metadata
    zone.set_property("crop", "wheat");
    zone.set_property("season", "spring");
    
    // 4. Check point containment
    bool inside = zone.poly().contains(concord::Point{50, 25, 0});
    
    // 5. Add raster data (elevation, moisture, etc.)
    concord::Grid<uint8_t> elevation_grid(100, 50, 1.0, true, concord::Pose{});
    zone.add_raster_layer(elevation_grid, "elevation", "terrain");
    
    // 6. Add features (obstacles, waypoints)
    concord::Polygon obstacle;
    obstacle.addPoint(concord::Point{20, 20, 0});
    obstacle.addPoint(concord::Point{25, 20, 0});
    obstacle.addPoint(concord::Point{25, 25, 0});
    obstacle.addPoint(concord::Point{20, 25, 0});
    zone.add_polygon_feature(obstacle, "tree", "obstacle");
    
    // 7. Save and load
    zone.save("my_zone");
    auto loaded = zoneout::Zone::load("my_zone");
    
    return 0;
}
```

## Core Concepts

- **Zone** = Workspace (field/barn) + Vector features + Multi-layer raster data
- **Poly** = Vector geometry (boundaries, features) with structured elements
- **Grid** = Multi-layer raster data (elevation, moisture, vegetation)
- **Plot** = Collection of zones for farm-wide management
- **Coordinate Systems**: ENU (local meters) + WGS84 (GPS positioning)
- **Resolution**: Trade-off between detail (0.1m) and performance (5.0m)

## API Reference

### Zone API
```cpp
// Creation
Zone(name, type, boundary, datum, resolution);

// Properties
zone.set_property(key, value);
zone.get_property(key);

// Vector geometry access
zone.poly().contains(point);
zone.poly().area();
zone.poly().perimeter();
zone.poly().has_field_boundary();

// Raster data
zone.add_raster_layer(grid, name, type, properties, poly_cut);
zone.raster_data();  // Access underlying geotiv::Raster

// Features
zone.add_polygon_feature(geometry, name, type, subtype, properties);
zone.feature_info();
zone.raster_info();

// I/O
zone.save(directory);
Zone::load(directory);
zone.to_files(vector_path, raster_path);
Zone::from_files(vector_path, raster_path);
```

### Grid API
```cpp
// Grid with metadata
Grid(name, type, subtype, datum, shift, resolution);

// Properties
grid.get_name();
grid.set_name(name);
grid.get_type();
grid.is_valid();

// Raster operations
grid.add_grid(width, height, name, type, properties);
grid.add_grid(grid_data, name, type, properties);

// I/O
Grid::from_file(path);
grid.to_file(path);
```

### Poly API
```cpp
// Vector data with structured elements
Poly(name, type, subtype, boundary, datum, heading, crs);

// Properties
poly.get_name();
poly.set_name(name);
poly.is_valid();
poly.has_field_boundary();

// Geometry
poly.area();
poly.perimeter();
poly.contains(point);

// Structured elements
poly.add_polygon_element(id, name, type, subtype, geometry, props);
poly.add_line_element(id, name, type, subtype, geometry, props);
poly.add_point_element(id, name, type, subtype, geometry, props);
poly.get_polygon_elements();
poly.get_polygons_by_type(type);

// I/O
Poly::from_file(path);
poly.to_file(path, crs);
```

### Plot API
```cpp
// Multi-zone management
Plot(name, type, datum);

// Zone management
plot.add_zone(zone);
plot.remove_zone(zone_id);
plot.get_zone(zone_id);
plot.get_zones();
plot.get_zone_count();

// Properties
plot.set_property(key, value);
plot.get_property(key);

// I/O
plot.save(directory);
plot.save_tar(tar_file);
Plot::load(directory, name, type, datum);
Plot::load_tar(tar_file, name, type, datum);
```

## Architecture

```
┌─────────────────────────────────────────┐
│              Plot                       │  Farm-level management
│  (Collection of Zones)                  │
└──────────────┬──────────────────────────┘
               │
               ├──► Zone = Poly + Grid    │  Field-level workspace
               │     ├─ Vector features   │
               │     └─ Raster layers     │
               │
               ├──► Poly (geoson::Vector) │  Boundaries & features
               │     └─ Structured elements│
               │
               └──► Grid (geotiv::Raster) │  Multi-layer data
                     └─ Elevation, etc.   │
```

## Dependencies

- **concord**: Core geometry primitives (Point, Polygon, Grid, Layer)
- **geoson**: Vector data I/O (GeoJSON format)
- **geotiv**: Raster data I/O (GeoTIFF format)
- **entropy**: UUID generation
- **doctest**: Unit testing framework
- **C++20**: Modern language features

## License

MIT License - See LICENSE file for details.

---

**Built for autonomous agriculture** 🌾🤖
