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
    // 1. Create boundary (100m × 50m)
    concord::Polygon boundary;
    boundary.addPoint(concord::Point{0, 0, 0});
    boundary.addPoint(concord::Point{100, 0, 0});
    boundary.addPoint(concord::Point{100, 50, 0});
    boundary.addPoint(concord::Point{0, 50, 0});
    
    // 2. Create zone with GPS datum and 1m resolution
    concord::Datum datum{52.0, 5.0, 0.0};  // lat, lon, alt
    zoneout::Zone zone("field", "agricultural", boundary, datum, 1.0);
    
    // 3. Add properties and features
    zone.set_property("crop", "wheat");
    
    // 4. Check point containment
    bool inside = zone.poly().contains(concord::Point{50, 25, 0});
    
    // 5. Path planning with 3D occlusion
    zone.initialize_occlusion_layer(10, 1.0);  // 10 layers × 1m height
    bool clear = zone.is_path_clear(start, end, robot_height, threshold);
    
    // 6. Persist
    zone.save("my_zone");
    auto loaded = zoneout::Zone::load("my_zone");
    
    return 0;
}
```

## Core Concepts

- **Zone** = Workspace (field/barn) + Internal features + Multi-layer raster data
- **Coordinate Systems**: ENU (local meters) + WGS84 (GPS positioning)
- **Resolution**: Trade-off between detail (0.1m) and performance (5.0m)
- **Occlusion Layer**: 3D grid for robot path planning and obstacle avoidance

## API Reference

```cpp
// Zone creation
Zone(name, type, boundary, datum, resolution);

// Properties
zone.set_property(key, value);
zone.get_property(key);

// Geometry queries
zone.poly().contains(point);
zone.poly().area();

// Raster layers
zone.add_raster_layer(grid, name, type, properties, poly_cut);

// 3D path planning
zone.initialize_occlusion_layer(height_layers, layer_height);
zone.is_path_clear(start, end, robot_height, threshold);

// Features
zone.add_polygon_feature(geometry, name, type, subtype, properties);

// File I/O
zone.save(directory);
Zone::load(directory);
zone.to_files(vector_path, raster_path);
Zone::from_files(vector_path, raster_path);
```

## Dependencies

- **concord**: Geometry (Point, Polygon, Grid)
- **geoson**: Vector data (GeoJSON)
- **geotiv**: Raster data (GeoTIFF)
- **C++20**: Modern features

## License

MIT License - See LICENSE file for details.

---

**Built for autonomous agriculture** 🌾🤖
