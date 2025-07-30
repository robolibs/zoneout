# ISOXML Generation from Zoneout Zones

This document describes how to implement ISOXML/taskmap generation from zoneout zones, supporting both raster (prescription maps) and vector data (AB-lines, boundaries, guidance patterns).

## Overview

ISOXML (ISO 11783-10) is the standard format for agricultural task data exchange. It consists of:
- XML files describing tasks, fields, and prescriptions
- Binary files containing grid/raster data
- Support for both planned (prescription) and actual (as-applied) data

## Core Components

### 1. ISOXML Structure

```
TASKDATA/
├── TASKDATA.XML          # Main task definition file
├── GRD00001.BIN         # Binary grid data (prescription maps)
├── GRD00001.XML         # Grid metadata
├── GRD00002.BIN         # Additional grid layers
└── GRD00002.XML
```

### 2. Key ISOXML Elements for Zoneout

- **TSK (Task)**: Maps to a Zone
- **PFD (Partfield)**: Field boundary from zone.poly_data_
- **TZN (TreatmentZone)**: Polygon features with application rates
- **GRD (Grid)**: Raster layers from zone.grid_data_
- **GPN (GuidancePattern)**: AB-lines and guidance paths
- **PLN (Polygon)**: Vector geometries
- **LSG (LineString)**: Paths, AB-lines, boundaries
- **PNT (Point)**: Reference points, flags

## Implementation Design

### ISOXMLConverter Class

```cpp
#pragma once
#include "zoneout/zone.hpp"
#include <tinyxml2.h>
#include <filesystem>

namespace zoneout {

class ISOXMLConverter {
public:
    struct ConversionConfig {
        std::string manufacturer = "Zoneout";
        std::string software_version = "1.0";
        int version_major = 4;
        int version_minor = 3;
        bool include_guidance_patterns = true;
        bool include_prescriptions = true;
    };

private:
    ConversionConfig config_;
    int grid_counter_ = 1;
    int tzn_counter_ = 1;
    int pattern_counter_ = 1;

public:
    ISOXMLConverter(const ConversionConfig& config = {}) : config_(config) {}

    // Main conversion function
    void convertZoneToTaskData(const Zone& zone, const std::filesystem::path& output_dir);

private:
    // XML generation
    void generateTaskDataXML(const Zone& zone, const std::filesystem::path& xml_path);
    
    // Grid/Raster conversion
    void exportGridLayers(const Grid& grid_data, const std::filesystem::path& taskdata_dir);
    void writeGridBinary(const concord::Grid<uint8_t>& grid, const std::filesystem::path& bin_path, int grid_type);
    void writeGridXML(const concord::Grid<uint8_t>& grid, const std::filesystem::path& xml_path, 
                      const std::string& grid_filename, int grid_type);
    
    // Vector conversion
    void addFieldBoundary(tinyxml2::XMLElement* task_elem, const Poly& poly_data);
    void addTreatmentZones(tinyxml2::XMLElement* task_elem, const Poly& poly_data);
    void addGuidancePatterns(tinyxml2::XMLElement* task_elem, const Poly& poly_data);
    
    // Coordinate conversion
    int32_t toISOXMLCoordinate(double coordinate);
    std::string formatISOXMLPosition(const concord::Point& point);
};

}
```

### Key Implementation Functions

#### 1. Main Conversion Function

```cpp
void ISOXMLConverter::convertZoneToTaskData(const Zone& zone, const std::filesystem::path& output_dir) {
    // Create TASKDATA directory
    auto taskdata_dir = output_dir / "TASKDATA";
    std::filesystem::create_directories(taskdata_dir);
    
    // Export grid layers as binary files
    if (config_.include_prescriptions && zone.grid_data_.hasGrids()) {
        exportGridLayers(zone.grid_data_, taskdata_dir);
    }
    
    // Generate main TASKDATA.XML
    generateTaskDataXML(zone, taskdata_dir / "TASKDATA.XML");
}
```

#### 2. Grid Export (Prescription Maps)

```cpp
void ISOXMLConverter::exportGridLayers(const Grid& grid_data, const std::filesystem::path& taskdata_dir) {
    // Export each raster layer as a separate grid
    for (size_t i = 0; i < grid_data.gridCount(); ++i) {
        const auto& layer = grid_data.getGrid(i);
        std::string grid_filename = fmt::format("GRD{:05d}", grid_counter_++);
        
        // Write binary grid data
        auto bin_path = taskdata_dir / (grid_filename + ".BIN");
        writeGridBinary(layer.grid, bin_path, 1); // Type 1: Treatment zone codes
        
        // Write grid metadata XML
        auto xml_path = taskdata_dir / (grid_filename + ".XML");
        writeGridXML(layer.grid, xml_path, grid_filename, 1);
    }
}
```

#### 3. Vector Data Handling

```cpp
void ISOXMLConverter::addGuidancePatterns(tinyxml2::XMLElement* task_elem, const Poly& poly_data) {
    auto doc = task_elem->GetDocument();
    
    // Extract AB-lines from line elements
    for (const auto& [id, element] : poly_data.getLineElements()) {
        if (element.type == "guidance" || element.subtype == "ab-line") {
            auto gpn = doc->NewElement("GPN");
            gpn->SetAttribute("A", fmt::format("{}", pattern_counter_++).c_str());
            gpn->SetAttribute("B", element.name.c_str());
            gpn->SetAttribute("C", "1"); // Type: AB-line
            
            // Add line string
            auto lsg = doc->NewElement("LSG");
            lsg->SetAttribute("A", "1"); // Type: LineString
            
            // Add points
            for (const auto& point : element.geometry.getPoints()) {
                auto pnt = doc->NewElement("PNT");
                pnt->SetAttribute("A", "2"); // Type: Other
                pnt->SetAttribute("C", toISOXMLCoordinate(point.y)); // North
                pnt->SetAttribute("D", toISOXMLCoordinate(point.x)); // East
                lsg->InsertEndChild(pnt);
            }
            
            gpn->InsertEndChild(lsg);
            task_elem->InsertEndChild(gpn);
        }
    }
}
```

## Data Mapping

### Zone to ISOXML Mapping

| Zoneout Data | ISOXML Element | Notes |
|--------------|----------------|-------|
| Zone boundary | PFD (Partfield) | Field boundary polygon |
| Grid layers | GRD (Grid) | Binary prescription maps |
| Polygon features | TZN (TreatmentZone) | Management zones |
| Line features (AB-lines) | GPN (GuidancePattern) | Type 1 = AB-line |
| Line features (paths) | GPN (GuidancePattern) | Type 2 = A+ heading |
| Point features | PNT (Point) | Flags, reference points |
| Zone properties | Task attributes | Custom properties as CTR |

### Coordinate System

```cpp
// ISOXML uses decimal degrees × 10^7 as signed 32-bit integers
int32_t ISOXMLConverter::toISOXMLCoordinate(double coordinate) {
    return static_cast<int32_t>(std::round(coordinate * 10000000.0));
}
```

### Grid Type Mapping

- **Type 1 Grid**: Treatment Zone Codes (8-bit values referencing TZN elements)
- **Type 2 Grid**: Actual prescription values (32-bit values with DDI)

## Usage Example

```cpp
#include "zoneout/isoxml_converter.hpp"

// Load a zone
Zone zone = Zone::fromFiles("field.json", "field.tif");

// Configure converter
ISOXMLConverter::ConversionConfig config;
config.manufacturer = "MyFarmSoftware";
config.include_guidance_patterns = true;

// Convert to ISOXML
ISOXMLConverter converter(config);
converter.convertZoneToTaskData(zone, "output");

// Result: output/TASKDATA/TASKDATA.XML with all associated files
```

## Advanced Features

### 1. Multi-Layer Prescription Support

Each grid layer in zone.grid_data_ becomes a separate GRD element:
- Base layer: Field boundary mask
- Additional layers: Fertilizer rates, seed rates, etc.

### 2. AB-Line Extraction

Line elements with type="guidance" or subtype="ab-line" are converted to:
```xml
<GPN A="1" B="AB Line North" C="1">
  <LSG A="1">
    <PNT A="2" C="581234567" D="85678901"/>
    <PNT A="2" C="581234567" D="85679901"/>
  </LSG>
</GPN>
```

### 3. Treatment Zone Generation

Polygon features become treatment zones with prescription values:
```xml
<TZN A="1" B="High Yield Zone" C="3">
  <PDV A="0006" B="15000" C="PDT1"/>
  <PLN A="1">
    <LSG A="1">
      <!-- Polygon points -->
    </LSG>
  </PLN>
</TZN>
```

## Validation

1. Ensure XML validates against ISO 11783-10 schema
2. Grid binary files match declared dimensions
3. Coordinate values are within valid ranges
4. Treatment zone codes are 0-254
5. All referenced files exist in TASKDATA directory

## Extensions

- Add support for curved guidance patterns (Type 3, 4)
- Implement time-based prescriptions
- Support for section control data
- Integration with ISOBUS data dictionary (DDI codes)