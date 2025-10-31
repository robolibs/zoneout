# Zoneout: Agricultural Zone Management Library

## **Multi-dimensional Spatial Data Management for Agricultural Robotics**

---

### **Core Architecture**
**Unified data model combining vector, raster, and 3D volumetric data:**
- **Vector Layer (Poly)**: Field boundaries, crop rows, irrigation lines, waypoints
- **Raster Layer (Grid)**: Temperature, moisture, elevation, soil properties  
- **3D Layer (Map)**: Height-aware occlusion maps for robot navigation

### **Key Features**
- **📍 Geospatial Integration**: WGS84 ↔ ENU coordinate transformations
- **💾 Standard Formats**: GeoJSON (vector) + GeoTIFF (raster + 3D maps)
- **🔧 Header-only C++**: Zero external dependencies, cmake integration
- **🤖 Robot-ready**: Path planning, obstacle detection, safe height analysis

---

### **Scientific Applications**

| **Domain** | **Use Case** | **Data Types** |
|------------|--------------|----------------|
| **Precision Agriculture** | Variable-rate applications | Raster: soil maps, yield data<br>Vector: field boundaries, management zones |
| **Autonomous Navigation** | Robot path planning | 3D Map: obstacle heights, clearance zones<br>Vector: safe corridors, no-go areas |
| **Environmental Monitoring** | Multi-sensor data fusion | Raster: spectral indices, temperature<br>Vector: sample locations, transects |
| **Research Workflows** | Reproducible field experiments | Unified format: experimental plots + sensor data |

---

### **Implementation Example**
```cpp
// Create agricultural zone with multi-modal data
zoneout::Zone field("Research_Plot", "agricultural", boundary, datum, 0.5);

// Add 2D environmental data  
field.addRasterLayer(temperature_grid, "temperature", "environmental");
field.addRasterLayer(moisture_grid, "soil_moisture", "environmental");

// Add 3D obstacle map for robots
field.initializeOcclusionLayer(20, 1.0); // 20 layers, 1m height resolution

// Robot navigation queries
bool path_clear = field.isPathClear(start_pos, goal_pos, robot_height);
double safe_height = field.getOcclusionLayer().findSafeHeight(x, y);
```

---

### **File Structure & Interoperability**
```
research_field/
├── vector.geojson    # Field boundaries, experimental plots (QGIS/ArcGIS)
├── raster.tiff       # Multi-band environmental data (GDAL/Python)
└── map.tiff          # 3D obstacle data (custom robotics tools)
```

**Standards compliant** → Direct integration with existing GIS workflows and scientific computing pipelines