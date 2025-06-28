#include <iostream>
#include "zoneout/zoneout.hpp"

int main() {
    std::cout << "Enhanced Zone Example with Vector/Raster Integration\n" << std::endl;
    
    // Create a basic field boundary
    std::vector<concord::Point> field_points;
    field_points.emplace_back(0.0, 0.0, 0.0);
    field_points.emplace_back(200.0, 0.0, 0.0);
    field_points.emplace_back(200.0, 100.0, 0.0);
    field_points.emplace_back(0.0, 100.0, 0.0);
    concord::Polygon field_boundary(field_points);
    
    // Create zone with enhanced Vector data
    auto zone = zoneout::Zone::createField("Smart Farm Field", field_boundary);
    
    // ========== Vector Integration (Field Elements) ==========
    std::cout << "=== Adding Field Elements (Vector Data) ===" << std::endl;
    
    // Add irrigation lines
    std::vector<concord::Point> irrigation_points;
    irrigation_points.emplace_back(10.0, 25.0, 0.0);
    irrigation_points.emplace_back(190.0, 25.0, 0.0);
    concord::Path irrigation_line(irrigation_points);
    
    std::unordered_map<std::string, std::string> irrigation_props;
    irrigation_props["flow_rate"] = "50L/min";
    irrigation_props["pressure"] = "2.5bar";
    zone.addFieldElement(irrigation_line, "irrigation_line", irrigation_props);
    
    // Add crop rows as line elements
    for (int row = 0; row < 10; ++row) {
        std::vector<concord::Point> row_points;
        double y = 10.0 + row * 8.0;
        row_points.emplace_back(5.0, y, 0.0);
        row_points.emplace_back(195.0, y, 0.0);
        concord::Path crop_row(row_points);
        
        std::unordered_map<std::string, std::string> row_props;
        row_props["crop_type"] = "wheat";
        row_props["planting_date"] = "2024-03-15";
        row_props["row_number"] = std::to_string(row + 1);
        
        zone.addFieldElement(crop_row, "crop_row", row_props);
    }
    
    // Add obstacles (trees, rocks, etc.)
    concord::Point tree_location(50.0, 30.0, 0.0);
    std::unordered_map<std::string, std::string> tree_props;
    tree_props["obstacle_type"] = "oak_tree";
    tree_props["height"] = "15m";
    zone.addFieldElement(tree_location, "obstacle", tree_props);
    
    std::cout << "Added field elements:" << std::endl;
    auto irrigation_elements = zone.getFieldElements("irrigation_line");
    std::cout << "- Irrigation lines: " << irrigation_elements.size() << std::endl;
    
    auto crop_rows = zone.getFieldElements("crop_row");
    std::cout << "- Crop rows: " << crop_rows.size() << std::endl;
    
    auto obstacles = zone.getFieldElements("obstacle");
    std::cout << "- Obstacles: " << obstacles.size() << std::endl;
    
    // ========== Raster Integration (Multi-layer Grid Data) ==========
    std::cout << "\n=== Adding Raster Layers (Grid Data) ===" << std::endl;
    
    // Create elevation layer
    concord::Grid<uint8_t> elevation_grid(20, 40, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 20; ++r) {
        for (size_t c = 0; c < 40; ++c) {
            uint8_t elevation = 100 + (r * c / 10);  // Simple elevation gradient
            elevation_grid.set_value(r, c, elevation);
        }
    }
    
    std::unordered_map<std::string, std::string> elevation_props;
    elevation_props["units"] = "meters";
    elevation_props["datum"] = "sea_level";
    zone.addRasterLayer("elevation", "height", elevation_grid, elevation_props);
    
    // Create soil moisture layer
    concord::Grid<uint8_t> moisture_grid(20, 40, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 20; ++r) {
        for (size_t c = 0; c < 40; ++c) {
            uint8_t moisture = 30 + (r + c) % 40;  // Varying moisture levels
            moisture_grid.set_value(r, c, moisture);
        }
    }
    
    std::unordered_map<std::string, std::string> moisture_props;
    moisture_props["units"] = "percentage";
    moisture_props["sensor_type"] = "capacitive";
    zone.addRasterLayer("soil_moisture", "moisture", moisture_grid, moisture_props);
    
    std::cout << "Added raster layers:" << std::endl;
    std::cout << "- Elevation data: 20x40 grid" << std::endl;
    std::cout << "- Soil moisture: 20x40 grid" << std::endl;
    
    // ========== Enhanced Data Access ==========
    std::cout << "\n=== Enhanced Zone Capabilities ===" << std::endl;
    
    // Check if enhanced data is available
    std::cout << "Has Vector data: " << (zone.hasFieldBoundary() ? "Yes" : "No") << std::endl;
    std::cout << "Has Raster data: " << (zone.numRasterLayers() > 0 ? "Yes" : "No") << std::endl;
    
    // Access vector data
    if (zone.hasFieldBoundary()) {
        auto field_elements = zone.getFieldElements();
        std::cout << "Total field elements: " << field_elements.size() << std::endl;
    }
    
    // Access raster data  
    if (zone.numRasterLayers() > 0) {
        auto layer_names = zone.getRasterLayerNames();
        std::cout << "Raster layers: " << layer_names.size() << std::endl;
    }
    
    // Modern raster layer access
    std::cout << "\n=== Raster Layer Details ===" << std::endl;
    std::cout << "Raster layer count: " << zone.numRasterLayers() << std::endl;
    for (const auto& layer_name : zone.getRasterLayerNames()) {
        std::cout << "- Raster Layer: " << layer_name << std::endl;
    }
    
    // ========== File I/O with Enhanced Format ==========
    std::cout << "\n=== Enhanced File I/O ===" << std::endl;
    
    try {
        // Save using new enhanced format (Vector + Raster)
        zone.toFiles("/tmp/enhanced_field.geojson", "/tmp/enhanced_field.tiff");
        std::cout << "Zone saved to enhanced format files" << std::endl;
        
        // Load back from enhanced format
        auto loaded_zone = zoneout::Zone::fromFiles("/tmp/enhanced_field.geojson", "/tmp/enhanced_field.tiff");
        std::cout << "Zone loaded from enhanced format files" << std::endl;
        std::cout << "Loaded zone name: " << loaded_zone.getName() << std::endl;
        std::cout << "Loaded vector elements: " << loaded_zone.getFieldElements().size() << std::endl;
        std::cout << "Loaded raster layers: " << loaded_zone.numRasterLayers() << std::endl;
                     
    } catch (const std::exception& e) {
        std::cout << "File I/O error: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== Enhanced Zone Example Complete! ===" << std::endl;
    return 0;
}