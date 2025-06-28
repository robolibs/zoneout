#include <iostream>
#include "zoneout/zoneout.hpp"

// Helper function to create a rectangle
concord::Polygon createRectangle(double x, double y, double width, double height) {
    std::vector<concord::Point> points;
    points.emplace_back(x, y, 0.0);
    points.emplace_back(x + width, y, 0.0);
    points.emplace_back(x + width, y + height, 0.0);
    points.emplace_back(x, y + height, 0.0);
    return concord::Polygon(points);
}

int main() {
    std::cout << "Farm Management Example\n" << std::endl;
    
    // Create a farm
    zoneout::Farm smart_farm("Smart Agricultural Farm");
    
    // ========== Create Different Zone Types ==========
    std::cout << "=== Creating Farm Zones ===" << std::endl;
    
    // Create fields
    auto& field1 = smart_farm.createField("North Field", createRectangle(0, 0, 200, 100));
    field1.setProperty("crop_type", "wheat");
    field1.setProperty("planting_date", "2024-03-15");
    
    auto& field2 = smart_farm.createField("South Field", createRectangle(0, -150, 200, 100));
    field2.setProperty("crop_type", "corn");
    field2.setProperty("planting_date", "2024-04-01");
    
    // Create barn
    auto& barn = smart_farm.createBarn("Main Barn", createRectangle(250, 0, 50, 40));
    barn.setProperty("animal_type", "dairy_cows");
    barn.setProperty("capacity", "100");
    
    // Create greenhouse
    auto& greenhouse = smart_farm.createGreenhouse("Tomato Greenhouse", createRectangle(250, 50, 30, 20));
    greenhouse.setProperty("crop_type", "tomatoes");
    greenhouse.setProperty("climate_control", "automated");
    
    std::cout << "Created " << smart_farm.numZones() << " zones:" << std::endl;
    std::cout << "- Fields: " << smart_farm.numZonesByType("field") << std::endl;
    std::cout << "- Barns: " << smart_farm.numZonesByType("barn") << std::endl;
    std::cout << "- Greenhouses: " << smart_farm.numZonesByType("greenhouse") << std::endl;
    
    // ========== Add Raster Data to Fields ==========
    std::cout << "\n=== Adding Raster Data ===" << std::endl;
    
    // Add elevation data to field1
    concord::Grid<uint8_t> elevation_grid(20, 40, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 20; ++r) {
        for (size_t c = 0; c < 40; ++c) {
            elevation_grid.set_value(r, c, 95 + (r + c) % 20);
        }
    }
    field1.add_layer("elevation", "terrain", elevation_grid, {{"units", "meters"}});
    
    // Add soil moisture data to field1
    concord::Grid<uint8_t> moisture_grid(20, 40, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 20; ++r) {
        for (size_t c = 0; c < 40; ++c) {
            moisture_grid.set_value(r, c, 25 + (r * c) % 50);
        }
    }
    field1.add_layer("soil_moisture", "environmental", moisture_grid, {{"units", "percentage"}});
    
    std::cout << "Added raster layers to " << field1.getName() << ":" << std::endl;
    for (const auto& layer_name : field1.get_layer_names()) {
        std::cout << "- " << layer_name << std::endl;
    }
    
    // ========== Add Field Elements ==========
    std::cout << "\n=== Adding Field Elements ===" << std::endl;
    
    // Add crop rows to field1
    for (int row = 0; row < 10; ++row) {
        std::vector<concord::Point> row_points;
        double y = 10.0 + row * 8.0;
        row_points.emplace_back(5.0, y, 0.0);
        row_points.emplace_back(195.0, y, 0.0);
        concord::Path crop_row(row_points);
        
        std::unordered_map<std::string, std::string> row_props;
        row_props["crop_type"] = "wheat";
        row_props["row_number"] = std::to_string(row + 1);
        field1.add_element(crop_row, "crop_row", row_props);
    }
    
    // Add irrigation line
    std::vector<concord::Point> irrigation_points;
    irrigation_points.emplace_back(10.0, 50.0, 0.0);
    irrigation_points.emplace_back(190.0, 50.0, 0.0);
    concord::Path irrigation_line(irrigation_points);
    
    std::unordered_map<std::string, std::string> irrigation_props;
    irrigation_props["flow_rate"] = "75L/min";
    irrigation_props["pressure"] = "3.0bar";
    field1.add_element(irrigation_line, "irrigation_line", irrigation_props);
    
    std::cout << "Added field elements to " << field1.getName() << ":" << std::endl;
    std::cout << "- Crop rows: " << field1.get_elements("crop_row").size() << std::endl;
    std::cout << "- Irrigation lines: " << field1.get_elements("irrigation_line").size() << std::endl;
    
    // ========== Spatial Queries ==========
    std::cout << "\n=== Spatial Queries ===" << std::endl;
    
    // Test point queries
    concord::Point test_point1(100.0, 50.0, 0.0);  // Inside field1
    concord::Point test_point2(260.0, 20.0, 0.0);  // Inside barn
    concord::Point test_point3(400.0, 400.0, 0.0); // Outside all zones
    
    auto zones_at_point1 = smart_farm.findZonesContaining(test_point1);
    auto zones_at_point2 = smart_farm.findZonesContaining(test_point2);
    auto zones_at_point3 = smart_farm.findZonesContaining(test_point3);
    
    std::cout << "Point (100, 50) is in " << zones_at_point1.size() << " zones" << std::endl;
    if (!zones_at_point1.empty()) {
        std::cout << "  - " << zones_at_point1[0]->getName() << " (" << zones_at_point1[0]->getType() << ")" << std::endl;
        
        // Sample raster data at this point
        auto elevation = zones_at_point1[0]->sample_at("elevation", test_point1);
        auto moisture = zones_at_point1[0]->sample_at("soil_moisture", test_point1);
        if (elevation) std::cout << "  - Elevation: " << static_cast<int>(*elevation) << " meters" << std::endl;
        if (moisture) std::cout << "  - Soil moisture: " << static_cast<int>(*moisture) << "%" << std::endl;
    }
    
    std::cout << "Point (260, 20) is in " << zones_at_point2.size() << " zones" << std::endl;
    if (!zones_at_point2.empty()) {
        std::cout << "  - " << zones_at_point2[0]->getName() << " (" << zones_at_point2[0]->getType() << ")" << std::endl;
    }
    
    std::cout << "Point (400, 400) is in " << zones_at_point3.size() << " zones" << std::endl;
    
    // Find nearest zone
    auto nearest_to_point3 = smart_farm.findNearestZone(test_point3);
    if (nearest_to_point3) {
        std::cout << "Nearest zone to (400, 400): " << nearest_to_point3->getName() << std::endl;
    }
    
    // Find zones within radius
    auto zones_within_radius = smart_farm.findZonesWithinRadius(concord::Point(0, 0, 0), 150.0);
    std::cout << "Zones within 150m of origin: " << zones_within_radius.size() << std::endl;
    for (const auto* zone : zones_within_radius) {
        std::cout << "  - " << zone->getName() << std::endl;
    }
    
    // ========== Farm Statistics ==========
    std::cout << "\n=== Farm Statistics ===" << std::endl;
    
    std::cout << "Total farm area: " << smart_farm.totalArea() << " m²" << std::endl;
    std::cout << "Field area: " << smart_farm.areaByType("field") << " m²" << std::endl;
    std::cout << "Barn area: " << smart_farm.areaByType("barn") << " m²" << std::endl;
    std::cout << "Greenhouse area: " << smart_farm.areaByType("greenhouse") << " m²" << std::endl;
    
    auto bounding_box = smart_farm.getBoundingBox();
    if (bounding_box) {
        std::cout << "Farm bounding box: " 
                  << "(" << bounding_box->min_point.x << ", " << bounding_box->min_point.y << ") to "
                  << "(" << bounding_box->max_point.x << ", " << bounding_box->max_point.y << ")" << std::endl;
    }
    
    // ========== Zone Iteration ==========
    std::cout << "\n=== Zone Details ===" << std::endl;
    
    smart_farm.forEachZone([](const zoneout::Zone& zone) {
        std::cout << "Zone: " << zone.getName() << " (" << zone.getType() << ")" << std::endl;
        std::cout << "  Area: " << zone.area() << " m²" << std::endl;
        std::cout << "  Raster layers: " << zone.num_layers() << std::endl;
        std::cout << "  Field elements: " << zone.get_elements().size() << std::endl;
        
        // Show properties
        for (const auto& [key, value] : zone.getProperties()) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        std::cout << std::endl;
    });
    
    // ========== File I/O ==========
    std::cout << "=== File I/O ===" << std::endl;
    
    try {
        // Save farm to directory
        smart_farm.saveToDirectory("/tmp/smart_farm_zones");
        std::cout << "Farm saved to /tmp/smart_farm_zones/" << std::endl;
        
        // Load farm from directory
        auto loaded_farm = zoneout::Farm::loadFromDirectory("/tmp/smart_farm_zones", "Loaded Farm");
        std::cout << "Loaded farm with " << loaded_farm.numZones() << " zones" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "File I/O error: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== Farm Example Complete! ===" << std::endl;
    return 0;
}