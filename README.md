<img align="right" width="26%" src="./misc/logo.png">

Zoneout
===

A Header-Only C++ Library for Advanced Workspace Zone Engineering with Raster & Vector Map Integration

Zoneout provides comprehensive zone management for agricultural robotics, enabling distributed coordination, spatial reasoning, and efficient workspace partitioning for autonomous farming operations.

## 🚀 Overview

Zoneout is a modern C++ library designed for **agricultural robotics coordination** through intelligent zone management. It combines vector geometry (field boundaries, crop rows, irrigation) with raster data (elevation, soil moisture, vegetation health) to create a comprehensive spatial framework for autonomous farming operations.

### Core Concepts

```
📍 Zone = Vector Boundary + Raster Layers + Robot Coordination
┌─────────────────────────────────────────────────────────────┐
│  FARM: Multiple Zones with Spatial Indexing                │
│  ┌───────────────┐  ┌─────────────┐  ┌──────────────────┐  │
│  │   FIELD A     │  │   BARN      │  │   GREENHOUSE     │  │
│  │               │  │             │  │                  │  │
│  │ 🌾 Crop Rows  │  │ 🐄 Animals  │  │ 🍅 Controlled   │  │
│  │ 💧 Irrigation │  │ 📦 Storage  │  │ 🌡️  Environment │  │
│  │ 🚜 Robot #1   │  │             │  │ 🤖 Robot #3     │  │
│  └───────────────┘  └─────────────┘  └──────────────────┘  │
│            │                                               │
│  ┌─────────────────────────────────────────────────────────┼──
│  │ R-TREE SPATIAL INDEX: O(log n) zone queries            │
│  │ • Point containment: "Which zone contains robot?"      │
│  │ • Radius search: "Zones within 100m of position"      │
│  │ • Intersection: "Zones overlapping with path"         │
│  └─────────────────────────────────────────────────────────┘
└─────────────────────────────────────────────────────────────┘
```

## 🏗️ Architecture

### 🎯 Zone Structure
Each zone combines **vector** and **raster** data for complete spatial understanding:

```
┌─────────────────────────────────────────────────────────────┐
│                         ZONE                                │
├─────────────────────┬───────────────────────────────────────┤
│     VECTOR DATA     │           RASTER DATA                 │
│                     │                                       │
│ 🔲 Field Boundary   │ 📊 Multi-Layer Grids:                │
│ 🌾 Crop Rows        │   • Elevation (meters)               │
│ 💧 Irrigation Lines │   • Soil Moisture (%)                │
│ 🚧 Obstacles        │   • Vegetation Health (NDVI)         │
│ 🛤️  Access Paths    │   • Temperature (°C)                 │
│                     │   • Custom layers...                 │
│ Properties:         │                                       │
│ • Type: "field"     │ Sampling: sample_at(point)           │
│ • Crop: "wheat"     │ Resolution: 10m per pixel             │
│ • Owner: Robot #1   │ Positioning: GPS coordinates         │
└─────────────────────┴───────────────────────────────────────┘
```

### 🗺️ Coordinate System
Zoneout uses real-world coordinates with proper positioning:

```
         GPS Coordinates (WGS84)
             ↓
    Y (North) │ 
      ↑       │
      │   500,300 ──────────────────┐
      │       │                     │
      │   400 │     FIELD A         │ 200m
      │       │   (Robot #1)        │
      │   300 │                     │
      │       │                     │
      │   200 ├─────────────────────┤
      │       │     BARN            │ 100m  
      │   100 │   (Storage)         │
      │       │                     │
      │     0 └─────────────────────┘────→ X (East)
            0   100   200   300   500
                   500m
```

## 📦 Quick Start

### Installation
```bash
# Header-only library - just include!
git clone https://github.com/your-org/zoneout
cd zoneout
mkdir build && cd build
cmake .. && make
make test  # Verify everything works
```

### Basic Zone Creation
```cpp
#include "zoneout/zoneout.hpp"
using namespace zoneout;

// Create field boundary (GPS coordinates)
std::vector<concord::Point> boundary_points = {
    {0.0, 0.0, 0.0},      // Southwest corner
    {500.0, 0.0, 0.0},    // Southeast corner  
    {500.0, 300.0, 0.0},  // Northeast corner
    {0.0, 300.0, 0.0}     // Northwest corner
};
concord::Polygon field_boundary(boundary_points);

// Create agricultural zone
Zone wheat_field = Zone::createField("North Field", field_boundary);
wheat_field.setProperty("crop_type", "winter_wheat");
wheat_field.setProperty("planting_date", "2024-03-15");

std::cout << "Created field: " << wheat_field.getName() 
          << " (" << wheat_field.area() << " m²)" << std::endl;
// Output: Created field: North Field (150000 m²)
```

### Adding Field Elements
```cpp
// Add crop rows for precision farming
for (int row = 1; row <= 15; row++) {
    std::vector<concord::Point> row_path = {
        {10.0, 20.0 + row * 18.0, 0.0},   // Row start
        {490.0, 20.0 + row * 18.0, 0.0}   // Row end
    };
    
    std::unordered_map<std::string, std::string> row_props;
    row_props["row_number"] = std::to_string(row);
    row_props["seed_density"] = "120kg/hectare";
    
    wheat_field.add_element(concord::Path(row_path), "crop_row", row_props);
}

// Add irrigation system
std::vector<concord::Point> irrigation_line = {
    {50.0, 150.0, 0.0}, {450.0, 150.0, 0.0}
};
std::unordered_map<std::string, std::string> irrigation_props;
irrigation_props["flow_rate"] = "200L/min";
irrigation_props["coverage_width"] = "30m";

wheat_field.add_element(concord::Path(irrigation_line), "irrigation_line", irrigation_props);

// Add facilities (parking, storage, etc.)
concord::Polygon parking_area = createRectangle(520, 50, 30, 20);
std::unordered_map<std::string, std::string> parking_props;
parking_props["name"] = "main_parking";
parking_props["capacity"] = "5_vehicles";
parking_props["surface"] = "gravel";

wheat_field.add_element(parking_area, "parking_space", parking_props);

// Add storage area
concord::Polygon storage_area = createRectangle(80, 70, 25, 20);
std::unordered_map<std::string, std::string> storage_props;
storage_props["name"] = "equipment_storage";
storage_props["max_weight"] = "500kg_per_m2";
storage_props["weather_protection"] = "covered";

wheat_field.add_element(storage_area, "storage_area", storage_props);

// Add access routes  
std::vector<concord::Point> access_path = {{5, 50, 0}, {95, 50, 0}};
std::unordered_map<std::string, std::string> route_props;
route_props["name"] = "main_access";
route_props["width"] = "4m";
route_props["surface"] = "dirt_road";

wheat_field.add_element(concord::Path(access_path), "access_route", route_props);

// Add equipment points
concord::Point water_station(60, 40, 0);
std::unordered_map<std::string, std::string> station_props;
station_props["name"] = "water_station";
station_props["type"] = "irrigation_hub";
station_props["flow_rate"] = "100L_per_min";

wheat_field.add_element(water_station, "equipment_point", station_props);
```

### Raster Data Integration
```cpp
// Create elevation grid (25x50 cells, 10m resolution)
concord::Pose grid_pose;
grid_pose.point = concord::Point(250, 150, 0); // Center over field
concord::Grid<uint8_t> elevation_grid(25, 50, 10.0, true, grid_pose);

// Fill with elevation data (95-105 meters)
for (size_t row = 0; row < 25; row++) {
    for (size_t col = 0; col < 50; col++) {
        uint8_t elevation = 95 + (row + col) * 10 / 75; // Gentle slope
        elevation_grid.set_value(row, col, elevation);
    }
}

// Add to zone
wheat_field.add_layer("elevation", "terrain", elevation_grid, {{"units", "meters"}});

// Sample elevation at robot position
concord::Point robot_pos(250, 150, 0);
auto elevation = wheat_field.sample_at("elevation", robot_pos);
if (elevation) {
    std::cout << "Ground elevation: " << (int)*elevation << "m" << std::endl;
    // Output: Ground elevation: 99m
}
```

### Farm-Level Management
```cpp
// Create farm with multiple zones (snake_case API)
Farm smart_farm("Precision Agriculture Demo");

// Add zones to farm
smart_farm.add_zone(std::make_unique<Zone>(std::move(wheat_field)));
auto& barn = smart_farm.create_barn("Storage Barn", createRectangle(600, 0, 80, 60));
auto& greenhouse = smart_farm.create_greenhouse("Tomato House", createRectangle(0, 350, 200, 100));

std::cout << "Farm overview:" << std::endl;
std::cout << "• Total area: " << smart_farm.total_area() << " m²" << std::endl;
std::cout << "• Number of zones: " << smart_farm.num_zones() << std::endl;
std::cout << "• Field area: " << smart_farm.area_by_type("field") << " m²" << std::endl;
```

### Spatial Queries & Robot Coordination
```cpp
// Robot navigation queries (snake_case API)
concord::Point robot_position(250, 150, 0);

// Which zone contains the robot?
auto current_zones = smart_farm.find_zones_containing(robot_position);
if (!current_zones.empty()) {
    std::cout << "Robot is in: " << current_zones[0]->getName() << std::endl;
}

// Find zones within operational radius
auto nearby_zones = smart_farm.find_zones_within_radius(robot_position, 100.0);
std::cout << "Zones within 100m: " << nearby_zones.size() << std::endl;

// Query elements within the current zone
if (!current_zones.empty()) {
    auto parking_spaces = current_zones[0]->get_elements("parking_space");
    auto storage_areas = current_zones[0]->get_elements("storage_area");
    std::cout << "Available parking: " << parking_spaces.size() << std::endl;
    std::cout << "Storage areas: " << storage_areas.size() << std::endl;
    
    for (const auto& space : parking_spaces) {
        auto name_it = space.properties.find("name");
        auto capacity_it = space.properties.find("capacity");
        if (name_it != space.properties.end()) {
            std::cout << "- " << name_it->second;
            if (capacity_it != space.properties.end()) {
                std::cout << " (" << capacity_it->second << ")";
            }
            std::cout << std::endl;
        }
    }
}

// Find nearest facility for resupply
auto nearest_barn = smart_farm.find_nearest_zone(robot_position);
if (nearest_barn && nearest_barn->getType() == "barn") {
    std::cout << "Nearest resupply: " << nearest_barn->getName() << std::endl;
}

// Robot ownership for coordination
UUID harvester_robot = generateUUID();
current_zones[0]->setOwnerRobot(harvester_robot);
std::cout << "Zone assigned to robot: " << harvester_robot.toString() << std::endl;
```

## 🔧 Advanced Features

### 🌐 Distributed Coordination
```cpp
// Lamport logical clocks for distributed systems
LamportClock field_clock;
field_clock.tick(); // Local event
field_clock.update(42); // Sync with remote clock

// Zone ownership with automatic conflict resolution
if (!wheat_field.hasOwner()) {
    wheat_field.setOwnerRobot(robot_id);
    // Distributed algorithms can use Lamport timestamps for ordering
}
```

### 📊 Spatial Index Performance
```cpp
// R-tree spatial indexing for O(log n) queries
auto stats = smart_farm.getSpatialIndexStats();
std::cout << "Spatial index efficiency:" << std::endl;
std::cout << "• Indexed zones: " << stats.total_entries << std::endl;

// Efficient bulk operations
std::vector<concord::Point> survey_points = generateSurveyGrid(1000);
auto start = std::chrono::high_resolution_clock::now();

for (const auto& point : survey_points) {
    auto zones = smart_farm.findZonesContaining(point); // O(log n)
}

auto duration = std::chrono::high_resolution_clock::now() - start;
std::cout << "1000 point queries: " << duration.count() << "ms" << std::endl;
```

### 💾 Persistence & File I/O
```cpp
// Save zone data (dual-format: GeoJSON + GeoTIFF)
std::string vector_path = "/tmp/wheat_field.geojson";
std::string raster_path = "/tmp/wheat_field.tiff";

wheat_field.toFiles(vector_path, raster_path);
/*
Creates:
📄 wheat_field.geojson - Vector data (boundaries, elements, properties)
🗺️ wheat_field.tiff   - Raster data (elevation, soil data, etc.)
*/

// Load zone from files (round-trip preservation)
Zone loaded_field = Zone::fromFiles(vector_path, raster_path);
std::cout << "Loaded: " << loaded_field.getName() << std::endl;
std::cout << "Elements: " << loaded_field.get_elements().size() << std::endl;
std::cout << "Layers: " << loaded_field.num_layers() << std::endl;

// All data preserved through save/load cycle:
// ✓ Zone properties (name, type, custom properties)
// ✓ Vector elements (crop rows, irrigation, obstacles)
// ✓ Raster layers (elevation, soil moisture, NDVI)
// ✓ Grid data and spatial positioning
```

## 🎯 Use Cases

### 🚜 **Autonomous Farming**
```
Robot Task Planning:
┌─ Harvester Robot ─┐    ┌─ Sprayer Robot ──┐    ┌─ Survey Drone ──┐  
│ 1. Find crop rows │    │ 1. Check moisture │    │ 1. Scan fields  │
│ 2. Plan path      │    │ 2. Avoid obstacles│    │ 2. Update maps   │
│ 3. Claim zone     │    │ 3. Optimal routes │    │ 3. Share data    │
│ 4. Execute task   │    │ 4. Coordinate     │    │ 4. Monitor crops │
└───────────────────┘    └───────────────────┘    └──────────────────┘
         │                         │                         │
         └─────────── ZONEOUT COORDINATION ─────────────────┘
                    • Conflict-free zone ownership
                    • Real-time spatial queries  
                    • Environmental data sharing
```

### 🏭 **Precision Agriculture**
- **Variable Rate Application**: Use soil data to optimize fertilizer/pesticide application
- **Yield Monitoring**: Track harvest data per zone for next season planning
- **Irrigation Management**: Moisture sensors guide targeted watering schedules
- **Equipment Coordination**: Multiple robots work different zones simultaneously

### 🌱 **Research & Development**
- **Field Trials**: Compare treatment zones with statistical rigor
- **Sensor Fusion**: Combine satellite, drone, and ground sensor data
- **Digital Twins**: Virtual farm models for simulation and planning

## 📚 API Reference

### Zone Class
```cpp
class Zone {
    // Construction
    Zone(const std::string& name, const std::string& type, const concord::Polygon& boundary);
    static Zone createField(const std::string& name, const concord::Polygon& boundary);
    static Zone createBarn(const std::string& name, const concord::Polygon& boundary);
    static Zone createGreenhouse(const std::string& name, const concord::Polygon& boundary);
    
    // Geometry (snake_case API)
    double area() const;
    double perimeter() const;
    bool contains(const concord::Point& point) const;
    void set_boundary(const concord::Polygon& boundary);
    const concord::Polygon& get_boundary() const;
    bool has_boundary() const;
    
    // Field Elements (generic methods - user-defined types)
    void add_element(const geoson::Geometry& geom, const std::string& type, const Properties& props = {});
    std::vector<geoson::Element> get_elements(const std::string& type = "") const;
    
    // Raster Layers (generic methods - user-defined types)
    void add_layer(const std::string& name, const std::string& type, const concord::Grid<uint8_t>& grid, const Properties& props = {});
    std::optional<uint8_t> sample_at(const std::string& layer, const concord::Point& point) const;
    bool has_layer(const std::string& name) const;
    size_t num_layers() const;
    std::vector<std::string> get_layer_names() const;
    
    // Properties
    void setProperty(const std::string& key, const std::string& value);
    std::string getProperty(const std::string& key, const std::string& default_value = "") const;
    const std::unordered_map<std::string, std::string>& getProperties() const;
    
    // Robot Coordination
    void setOwnerRobot(const UUID& robot_id);
    bool hasOwner() const;
    void releaseOwnership();
    
    // Validation
    bool is_valid() const;
    
    // File I/O (dual-format persistence)
    void toFiles(const std::filesystem::path& vector_path, const std::filesystem::path& raster_path) const;
    static Zone fromFiles(const std::filesystem::path& vector_path, const std::filesystem::path& raster_path);
};
```

### Farm Class  
```cpp
class Farm {
    // Zone Management (snake_case API)
    void add_zone(std::unique_ptr<Zone> zone);
    Zone& create_field(const std::string& name, const concord::Polygon& boundary);
    Zone& create_barn(const std::string& name, const concord::Polygon& boundary);  
    Zone& create_greenhouse(const std::string& name, const concord::Polygon& boundary);
    size_t num_zones() const;
    
    // Spatial Queries (O(log n) with R-tree indexing)
    std::vector<Zone*> find_zones_containing(const concord::Point& point);
    std::vector<Zone*> find_zones_within_radius(const concord::Point& center, double radius);
    std::vector<Zone*> find_zones_intersecting(const concord::Polygon& area);
    Zone* find_nearest_zone(const concord::Point& point);
    
    // Statistics
    double total_area() const;
    double area_by_type(const std::string& type) const;
    std::optional<concord::AABB> get_bounding_box() const;
    
    // File I/O
    void save_to_directory(const std::filesystem::path& dir) const;
    static Farm load_from_directory(const std::filesystem::path& dir, const std::string& name);
};
```

## 🔧 Dependencies

- **[concord](https://github.com/your-org/concord)**: Geometry library (Point, Polygon, Grid, R-tree)
- **[geoson](https://github.com/your-org/geoson)**: Vector data handling (GeoJSON I/O)  
- **[geotiv](https://github.com/your-org/geotiv)**: Raster data handling (GeoTIFF I/O)
- **C++20**: Modern language features (concepts, modules, ranges)

## 🧪 Testing

```bash
# Run comprehensive test suite
make test

# Test categories:
# • Zone geometry and properties
# • Farm spatial indexing (R-tree)
# • Raster data sampling
# • Integration scenarios
# • Performance benchmarks
```

## 📈 Performance

- **Spatial Queries**: O(log n) with R-tree indexing vs O(n) brute force
- **Memory Usage**: Header-only design, minimal overhead
- **Scalability**: Tested with 1000+ zones, sub-millisecond queries
- **Concurrency**: Thread-safe UUID generation, read-only operations

## 🤝 Contributing

1. **Fork** the repository
2. **Create** feature branch (`git checkout -b feature/robot-swarms`)
3. **Commit** changes (`git commit -am 'Add robot swarm coordination'`)
4. **Push** to branch (`git push origin feature/robot-swarms`)  
5. **Create** Pull Request

## 📄 License

Licensed under the MIT License. See `LICENSE` file for details.

---

**Built for the future of autonomous agriculture** 🌾🤖

*Zoneout enables intelligent coordination between robots, environment, and crop management through spatial-aware zone engineering.*