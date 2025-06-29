#include <iostream>
#include <filesystem>
#include "zoneout/zoneout.hpp"

using namespace zoneout;

// Helper function to create a rectangular polygon
concord::Polygon createRectangle(double x, double y, double width, double height) {
    std::vector<concord::Point> points;
    points.emplace_back(x, y, 0.0);
    points.emplace_back(x + width, y, 0.0);
    points.emplace_back(x + width, y + height, 0.0);
    points.emplace_back(x, y + height, 0.0);
    return concord::Polygon(points);
}

int main() {
    std::cout << "=== Simple Raster Layer Test ===" << std::endl;
    
    // Create a simple zone
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary);
    
    // Set properties
    zone.setProperty("crop_type", "wheat");
    zone.setProperty("test_prop", "test_value");
    
    // Add one raster layer with actual data
    concord::Grid<uint8_t> test_grid(5, 10, 2.0, true, concord::Pose{});
    for (size_t r = 0; r < 5; ++r) {
        for (size_t c = 0; c < 10; ++c) {
            test_grid.set_value(r, c, static_cast<uint8_t>(100 + r * 10 + c));
        }
    }
    zone.add_layer("elevation", "terrain", test_grid, {{"units", "meters"}});
    
    std::cout << "Before save:" << std::endl;
    std::cout << "- Layers: " << zone.num_layers() << std::endl;
    std::cout << "- Has elevation: " << zone.has_layer("elevation") << std::endl;
    std::cout << "- crop_type: " << zone.getProperty("crop_type") << std::endl;
    
    // Save and load
    std::string vector_path = "/tmp/simple_test.geojson";
    std::string raster_path = "/tmp/simple_test.tiff";
    
    try {
        zone.toFiles(vector_path, raster_path);
        std::cout << "\nSaved successfully" << std::endl;
        
        std::cout << "Loading from files..." << std::endl;
        Zone loaded = Zone::fromFiles(vector_path, raster_path);
        std::cout << "Load completed!" << std::endl;
        std::cout << "\nAfter load:" << std::endl;
        std::cout << "- Name: " << loaded.getName() << std::endl;
        std::cout << "- Layers: " << loaded.num_layers() << std::endl;
        std::cout << "- Has elevation: " << loaded.has_layer("elevation") << std::endl;
        std::cout << "- crop_type: '" << loaded.getProperty("crop_type") << "'" << std::endl;
        
        if (loaded.num_layers() > 0) {
            std::cout << "SUCCESS: Raster layers working!" << std::endl;
        } else {
            std::cout << "FAIL: No raster layers loaded" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}