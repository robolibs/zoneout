<img align="right" width="26%" src="./misc/logo.png">

Zoneout
===

A Header-Only C++ Library for Advanced Workspace Zone Engineering with Raster & Vector Map Integration

Zoneout provides comprehensive zone management for agricultural robotics, enabling distributed coordination, spatial reasoning, and efficient workspace partitioning for autonomous farming operations.

## 🚀 Overview

Zoneout is a modern C++ library designed for **agricultural robotics coordination** through intelligent single-zone management. It combines vector geometry (zone boundaries, internal elements) with raster data (elevation, soil moisture, vegetation health) to create a comprehensive spatial framework for individual workspace management in autonomous farming operations.

Each Zone represents a single workspace (field, barn, greenhouse, etc.) containing polygon features as internal elements and multi-layer raster data for environmental information.

### Core Concepts

```
📍 Zone = Single Workspace (Field/Barn/etc.) + Internal Elements + Raster Data
┌─────────────────────────────────────────────────────────────┐
│                      SINGLE ZONE                           │
│                   (e.g., Field or Barn)                    │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │                ZONE BOUNDARY                            │ │
│  │                                                         │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐              │ │
│  │  │ Element  │  │ Element  │  │ Element  │              │ │
│  │  │ #1       │  │ #2       │  │ #3      │              │ │
│  │  │ 🌾 crop  │  │ 🚜 park  │  │ 💧 irrig │              │ │
│  │  │ area     │  │ space    │  │ system   │              │ │
│  │  └──────────┘  └──────────┘  └──────────┘              │ │
│  │                                                         │ │
│  │  POLYGON FEATURES (Elements within this zone):         │ │
│  │  • Automatic boundary validation (must fit in zone)    │ │
│  │  • Visual representation on raster base layer          │ │
│  │  • Functional areas (parking, storage, crop areas)     │ │
│  │  • Infrastructure elements (irrigation, paths)         │ │
│  └─────────────────────────────────────────────────────────┘ │
│                                                             │
│  RASTER LAYERS (Environmental data for this zone):         │
│  • Base layer: Generated from zone boundary                │
│  • Environmental data: Temperature, moisture, elevation    │
│  • Visualization: Elements drawn on base layer             │
└─────────────────────────────────────────────────────────────┘

Note: Multiple zones (Farm-level management) will be a future concept
```

## 🏗️ Architecture

### 🎯 Zone Structure
Each zone represents a **single workspace** with internal elements and environmental data:

```
┌─────────────────────────────────────────────────────────────┐
│                    SINGLE ZONE                             │
├─────────────────────┬───────────────────────────────────────┤
│   POLYGON FEATURES  │           RASTER DATA                 │
│   (Internal Elements)│                                      │
│                     │                                       │
│ 🔲 Zone Boundary    │ 📊 Multi-Layer Grids:                │
│ 🌾 Crop Areas       │   • Base layer (auto-generated)      │
│ 🚜 Parking Spaces   │   • Elevation (meters)               │
│ 📦 Storage Areas    │   • Soil Moisture (%)                │
│ 💧 Irrigation Systems│   • Temperature (°C)                │
│ 🛤️  Access Routes   │   • Custom layers...                 │
│                     │                                       │
│ Features:           │ Properties:                           │
│ • UUID identification│ • Automatic boundary cutting         │
│ • Type/subtype      │ • Resolution: configurable           │
│ • Custom metadata   │ • Positioning: GPS coordinates       │
│ • Boundary validation│ • Element visualization              │
└─────────────────────┴───────────────────────────────────────┘
```

### 🗺️ Coordinate System
Zoneout uses real-world coordinates with proper positioning:

```
         GPS Coordinates (WGS84)
             ↓
    Y (North) │ 
      ↑       │
      │   300 ┌─────────────────────────────┐
      │       │        FIELD ZONE           │
      │   250 │  ┌────┐  ┌────┐  ┌────┐    │ 
      │       │  │Crop│  │Park│  │Irrig│   │ 200m
      │   200 │  │Area│  │Spot│  │Zone│    │
      │       │  └────┘  └────┘  └────┘    │
      │   150 │                            │
      │       │     Single Zone with       │
      │   100 │     Internal Elements      │
      │       │                            │
      │    50 │                            │
      │       │                            │
      │     0 └────────────────────────────┘────→ X (East)
            0   50  100  150  200  250  300
                        300m

Note: Each Zone is independent. Multiple zones would be managed
      by a higher-level system (Future: Farm/Field management)
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

// Create zone boundary (GPS coordinates)
std::vector<concord::Point> boundary_points = {
    {0.0, 0.0, 0.0},      // Southwest corner
    {300.0, 0.0, 0.0},    // Southeast corner  
    {300.0, 200.0, 0.0},  // Northeast corner
    {0.0, 200.0, 0.0}     // Northwest corner
};
concord::Polygon zone_boundary(boundary_points);

// Create single agricultural zone with auto-generated base layer
const concord::Datum DATUM{51.98776, 5.662378, 0.0};
Zone field_zone("North Field", "field", zone_boundary, DATUM, 0.5);  // 0.5m resolution

// Set zone properties
field_zone.setProperty("crop_type", "winter_wheat");
field_zone.setProperty("planting_date", "2024-03-15");

std::cout << "Created zone: " << field_zone.getName() 
          << " (" << field_zone.poly_data_.area() << " m²)" << std::endl;
std::cout << "Raster info: " << field_zone.getRasterInfo() << std::endl;
// Output: Created zone: North Field (60000 m²)
// Output: Raster size: 400x600 (1 layers)
```

### Adding Polygon Features
```cpp
// Modern polygon feature API with automatic visualization
// Features are automatically drawn on the base raster layer with random colors (50-200)

// Create crop zones within the field
std::vector<concord::Point> corn_zone_points = {
    {50.0, 50.0, 0.0}, {200.0, 50.0, 0.0}, 
    {200.0, 150.0, 0.0}, {50.0, 150.0, 0.0}
};
concord::Polygon corn_zone(corn_zone_points);

std::unordered_map<std::string, std::string> corn_props = {
    {"crop_type", "corn"},
    {"management", "organic"},
    {"priority", "8"},
    {"season", "spring_2024"},
    {"area_m2", std::to_string(static_cast<int>(corn_zone.area()))}
};

// Add polygon feature - automatically validates boundaries and draws on base layer
wheat_field.addPolygonFeature(corn_zone, "corn_section_1", "agricultural", "crop_zone", corn_props);

// Create irrigation zone
std::vector<concord::Point> irrigation_zone_points = {
    {250.0, 100.0, 0.0}, {400.0, 100.0, 0.0},
    {400.0, 200.0, 0.0}, {250.0, 200.0, 0.0}
};
concord::Polygon irrigation_zone(irrigation_zone_points);

std::unordered_map<std::string, std::string> irrigation_props = {
    {"system_type", "drip_irrigation"},
    {"flow_rate", "200L/min"},
    {"coverage", "complete"},
    {"efficiency", "95_percent"}
};

wheat_field.addPolygonFeature(irrigation_zone, "irrigation_zone_1", "infrastructure", "irrigation", irrigation_props);

// Create equipment storage area
std::vector<concord::Point> storage_points = {
    {450.0, 50.0, 0.0}, {490.0, 50.0, 0.0},
    {490.0, 90.0, 0.0}, {450.0, 90.0, 0.0}
};
concord::Polygon storage_area(storage_points);

std::unordered_map<std::string, std::string> storage_props = {
    {"storage_type", "equipment"},
    {"capacity", "500kg_per_m2"},
    {"weather_protection", "covered"},
    {"access_level", "high"}
};

wheat_field.addPolygonFeature(storage_area, "equipment_storage", "facility", "storage", storage_props);

// Get feature information
std::cout << "Zone features: " << wheat_field.getFeatureInfo() << std::endl;
// Output: Features: 3 polygons, 0 lines, 0 points (3 total)
```

### Raster Data Integration
```cpp
// Create zone with automatic base layer generation
zoneout::Zone field("North Field", "field", field_boundary, WAGENINGEN_DATUM, 0.1);

// Access base grid for creating additional layers with same spatial configuration
const auto &base_grid = field.grid_data_.getGrid(0).grid;

// Create temperature grid with same spatial properties
auto temp_grid = base_grid;  // Copy spatial configuration
for (size_t r = 0; r < temp_grid.rows(); ++r) {
    for (size_t c = 0; c < temp_grid.cols(); ++c) {
        // Fill with temperature data (15-35°C range with noise)
        uint8_t temp_value = static_cast<uint8_t>(15 + noise_function(r, c) * 20);
        temp_grid.set_value(r, c, temp_value);
    }
}

// Add raster layers with poly_cut option
field.addRasterLayer(temp_grid, "temperature_full", "environmental", 
                     {{"units", "celsius"}}, false);  // Full coverage

// Create moisture grid
auto moisture_grid = base_grid;  // Copy spatial configuration
for (size_t r = 0; r < moisture_grid.rows(); ++r) {
    for (size_t c = 0; c < moisture_grid.cols(); ++c) {
        uint8_t moisture_value = static_cast<uint8_t>(20 + noise_function(r, c) * 60);
        moisture_grid.set_value(r, c, moisture_value);
    }
}

// Add with boundary cutting - zeros outside field boundary
field.addRasterLayer(moisture_grid, "soil_moisture", "environmental", 
                     {{"units", "percentage"}}, true);  // Cut to field shape

// Get raster information
std::cout << "Raster info: " << field.getRasterInfo() << std::endl;
// Output: Raster size: 50x30 (3 layers)

// Get feature information  
std::cout << "Feature info: " << field.getFeatureInfo() << std::endl;
// Output: Features: 3 polygons, 0 lines, 0 points (3 total)
```

### Zone Queries & Robot Coordination
```cpp
// Robot position tracking within zones
concord::Point robot_position(250, 150, 0);

// Check if robot is within field boundary
if (field.poly_data_.contains(robot_position)) {
    std::cout << "Robot is within field: " << field.getName() << std::endl;
}

// Query polygon features near robot position
const auto& polygon_elements = field.poly_data_.getPolygonElements();
for (const auto& element : polygon_elements) {
    if (element.geometry.contains(robot_position)) {
        std::cout << "Robot is in feature: " << element.name 
                  << " (type: " << element.type << ")" << std::endl;
        
        // Access feature properties
        auto crop_it = element.properties.find("crop_type");
        auto mgmt_it = element.properties.find("management");
        if (crop_it != element.properties.end()) {
            std::cout << "Crop type: " << crop_it->second << std::endl;
        }
        if (mgmt_it != element.properties.end()) {
            std::cout << "Management: " << mgmt_it->second << std::endl;
        }
    }
}

// Sample environmental data at robot position
if (field.grid_data_.gridCount() > 0) {
    const auto& base_grid = field.grid_data_.getGrid(0).grid;
    // Get grid cell value at robot position
    // Note: You would implement sampling logic based on your grid structure
    std::cout << "Environmental data available at position" << std::endl;
}

// Zone area and geometry queries
std::cout << "Field area: " << field.poly_data_.area() << " m²" << std::endl;
std::cout << "Field perimeter: " << field.poly_data_.perimeter() << " m" << std::endl;
```

## 🔧 Advanced Features

### 🌐 Zone-Level Operations
```cpp
// Zone boundary and feature validation
std::cout << "Zone valid: " << field.is_valid() << std::endl;

// Direct access to underlying data structures
// Vector data access
const auto& polygon_features = field.poly_data_.getPolygonElements();
const auto& line_features = field.poly_data_.getLineElements(); 
const auto& point_features = field.poly_data_.getPointElements();

// Raster data access
if (field.grid_data_.gridCount() > 0) {
    const auto& base_layer = field.grid_data_.getGrid(0);
    std::cout << "Base layer: " << base_layer.name << " (" << base_layer.type << ")" << std::endl;
}

// Feature filtering by type/subtype
auto crop_zones = field.poly_data_.getPolygonsByType("agricultural");
auto irrigation_zones = field.poly_data_.getPolygonsBySubtype("irrigation");

std::cout << "Found " << crop_zones.size() << " agricultural zones" << std::endl;
std::cout << "Found " << irrigation_zones.size() << " irrigation zones" << std::endl;
```

### 📊 Performance Characteristics
```cpp
// Zone operations are designed for efficiency:
// • Boundary validation: O(n) where n = polygon vertices
// • Feature lookup: Direct vector access
// • Grid sampling: O(1) cell access
// • File I/O: Dual-format (GeoJSON + GeoTIFF) with validation

// Memory usage is minimal:
// • Header-only library design
// • Efficient grid storage
// • UUID-based feature identification
// • Property maps for flexible metadata
```

### 💾 Persistence & File I/O
```cpp
// Save zone data (dual-format: GeoJSON + GeoTIFF)
field.toFiles("/tmp/field.geojson", "/tmp/field.tiff");
/*
Creates:
📄 field.geojson - Vector data (boundaries, polygon features, properties)
🗺️ field.tiff   - Raster data (base layer + additional layers)
*/

// Load zone from files (round-trip preservation)
auto loaded_field = zoneout::Zone::fromFiles("/tmp/field.geojson", "/tmp/field.tiff");
std::cout << "Loaded: " << loaded_field.getName() << std::endl;
std::cout << "Raster: " << loaded_field.getRasterInfo() << std::endl;
std::cout << "Features: " << loaded_field.getFeatureInfo() << std::endl;
std::cout << "Area preserved: " << (field.poly_data_.area() == loaded_field.poly_data_.area() ? "Yes" : "No") << std::endl;

// All data preserved through save/load cycle:
// ✓ Zone properties (name, type, custom properties)
// ✓ Polygon features with metadata and UUID tracking
// ✓ Raster layers with automatic boundary cutting support
// ✓ Grid data and spatial positioning
// ✓ Automatic visualization on base layer
```

### 🎯 Complete Example (from examples/main.cpp)
```cpp
#include "zoneout/zoneout.hpp"
#include "geoget/geoget.hpp"

int main() {
    // Set up coordinate system
    const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};
    
    // Interactive polygon drawing (web interface at localhost:8080)
    geoget::PolygonDrawer drawer(WAGENINGEN_DATUM);
    drawer.start(8080);
    const auto l_boundary = drawer.get_polygons();
    
    // Create field zone with automatic base layer generation (0.1m resolution)
    zoneout::Zone field("L-Shaped Field", "field", l_boundary[0], WAGENINGEN_DATUM, 0.1);
    
    // Create environmental raster layers
    const auto &base_grid = field.grid_data_.getGrid(0).grid;
    auto temp_grid = base_grid;      // Copy spatial configuration
    auto moisture_grid = base_grid;  // Copy spatial configuration
    
    // Fill with realistic environmental data using noise
    entropy::NoiseGen temp_noise, moisture_noise;
    temp_noise.SetNoiseType(entropy::NoiseGen::NoiseType_Perlin);
    moisture_noise.SetNoiseType(entropy::NoiseGen::NoiseType_OpenSimplex2);
    
    for (size_t r = 0; r < temp_grid.rows(); ++r) {
        for (size_t c = 0; c < temp_grid.cols(); ++c) {
            // Temperature: 15-35°C range
            float temp_noise_val = temp_noise.GetNoise(r, c);
            uint8_t temp_value = static_cast<uint8_t>(15 + (temp_noise_val + 1.0f) * 0.5f * 20);
            temp_grid.set_value(r, c, temp_value);
            
            // Moisture: 20-80% range
            float moisture_noise_val = moisture_noise.GetNoise(r, c);
            uint8_t moisture_value = static_cast<uint8_t>(20 + (moisture_noise_val + 1.0f) * 0.5f * 60);
            moisture_grid.set_value(r, c, moisture_value);
        }
    }
    
    // Add raster layers
    field.addRasterLayer(temp_grid, "temperature", "environmental", {{"units", "celsius"}}, false);
    field.addRasterLayer(moisture_grid, "soil_moisture", "environmental", {{"units", "percentage"}}, true);
    
    // Add polygon features from additional drawn boundaries
    for (size_t i = 1; i < l_boundary.size(); ++i) {
        std::unordered_map<std::string, std::string> properties = {
            {"crop_type", "corn"},
            {"management", "organic"},
            {"priority", "8"},
            {"season", "spring_2024"},
            {"area_m2", std::to_string(static_cast<int>(l_boundary[i].area()))}
        };
        
        std::string feature_name = "field_section_" + std::to_string(i);
        field.addPolygonFeature(l_boundary[i], feature_name, "agricultural", "crop_zone", properties);
    }
    
    // Display results
    std::cout << "Raster: " << field.getRasterInfo() << std::endl;
    std::cout << "Features: " << field.getFeatureInfo() << std::endl;
    
    // Save and load verification
    field.toFiles("/tmp/field.geojson", "/tmp/field.tiff");
    auto loaded = zoneout::Zone::fromFiles("/tmp/field.geojson", "/tmp/field.tiff");
    std::cout << "Round-trip successful: " << (field.poly_data_.area() == loaded.poly_data_.area()) << std::endl;
    
    return 0;
}
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
    Zone(const std::string& name, const std::string& type, const concord::Polygon& boundary,
         const concord::Grid<uint8_t>& initial_grid, const concord::Datum& datum);
    Zone(const std::string& name, const std::string& type, const concord::Polygon& boundary,
         const concord::Datum& datum, double resolution = 1.0);  // Auto-generates base grid
    
    // Basic Properties
    const UUID& getId() const;
    const std::string& getName() const;
    const std::string& getType() const;
    void setName(const std::string& name);
    void setType(const std::string& type);
    
    // Zone Properties
    void setProperty(const std::string& key, const std::string& value);
    std::string getProperty(const std::string& key, const std::string& default_value = "") const;
    const std::unordered_map<std::string, std::string>& getProperties() const;
    
    // Datum Management
    const concord::Datum& getDatum() const;
    void setDatum(const concord::Datum& datum);
    
    // Raster Layer Management
    void addRasterLayer(const concord::Grid<uint8_t>& grid, const std::string& name, 
                        const std::string& type = "", 
                        const std::unordered_map<std::string, std::string>& properties = {},
                        bool poly_cut = false, int layer_index = -1);
    std::string getRasterInfo() const;  // Returns layer count and dimensions
    
    // Polygon Feature Management (NEW)
    void addPolygonFeature(const concord::Polygon& geometry, const std::string& name,
                          const std::string& type = "", const std::string& subtype = "default",
                          const std::unordered_map<std::string, std::string>& properties = {});
    std::string getFeatureInfo() const;  // Returns feature counts by type
    
    // Direct Data Access
    Poly poly_data_;  // Vector data (boundaries + polygon features)
    Grid grid_data_;  // Raster data (multiple layers)
    
    // Validation
    bool is_valid() const;
    
    // File I/O (dual-format persistence)
    void toFiles(const std::filesystem::path& vector_path, const std::filesystem::path& raster_path) const;
    static Zone fromFiles(const std::filesystem::path& vector_path, const std::filesystem::path& raster_path);
};
```

### Key Features of New Polygon API
```cpp
// Automatic boundary validation - polygons must be inside field boundary
void addPolygonFeature(geometry, name, type, subtype, properties);

// Automatic visualization - features drawn on base layer with random colors (50-200)
// UUID generation - each feature gets unique identifier  
// Property storage - arbitrary key-value metadata
// Type system - organize features by type and subtype

// Feature information summary
std::string getFeatureInfo() const;
// Returns: "Features: 3 polygons, 2 lines, 1 points (6 total)"

// Raster information summary  
std::string getRasterInfo() const;
// Returns: "Raster size: 50x30 (4 layers)"
```

### Poly and Grid Classes (Direct Access)
```cpp
// Poly class - Vector data management
class Poly : public geoson::Vector {
    // Structured element access
    const std::vector<PolygonElement>& getPolygonElements() const;
    const std::vector<LineElement>& getLineElements() const;
    const std::vector<PointElement>& getPointElements() const;
    
    // Element filtering
    std::vector<PolygonElement> getPolygonsByType(const std::string& type) const;
    std::vector<PolygonElement> getPolygonsBySubtype(const std::string& subtype) const;
    
    // Geometry operations  
    double area() const;
    double perimeter() const;
    bool contains(const concord::Point& point) const;
    bool hasFieldBoundary() const;
    bool isValid() const;
};

// Grid class - Raster data management  
class Grid : public geotiv::Raster {
    // Grid access
    size_t gridCount() const;
    const GridLayer& getGrid(size_t index) const;
    
    // Grid management
    void addGrid(const concord::Grid<uint8_t>& grid, const std::string& name,
                 const std::string& type, const std::unordered_map<std::string, std::string>& properties = {});
    
    // Validation
    bool hasGrids() const;
    bool isValid() const;
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

- **Zone Operations**: Efficient boundary validation and feature management
- **Memory Usage**: Header-only design, minimal overhead
- **Grid Access**: O(1) cell access for raster data sampling
- **File I/O**: Optimized dual-format persistence (GeoJSON + GeoTIFF)
- **Feature Lookup**: Direct vector access for polygon elements
- **UUID Generation**: Thread-safe unique identifiers

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