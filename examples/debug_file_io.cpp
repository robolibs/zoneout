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
    std::cout << "=== Debug File I/O ===" << std::endl;
    
    // Create a simple zone
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Debug Zone", "field", boundary);
    
    // Set some properties
    zone.setProperty("crop_type", "wheat");
    zone.setProperty("test_prop", "test_value");
    
    std::cout << "Original zone properties:" << std::endl;
    std::cout << "- crop_type: " << zone.getProperty("crop_type") << std::endl;
    std::cout << "- test_prop: " << zone.getProperty("test_prop") << std::endl;
    
    // Add one element
    auto parking = createRectangle(10, 10, 20, 15);
    zone.add_element(parking, "parking", {{"capacity", "5_cars"}});
    
    // Add one raster layer
    concord::Grid<uint8_t> test_grid(10, 20, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 10; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            test_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
        }
    }
    zone.add_layer("elevation", "terrain", test_grid, {{"units", "meters"}});
    
    std::cout << "\nBefore save:" << std::endl;
    std::cout << "- Elements: " << zone.get_elements().size() << std::endl;
    std::cout << "- Layers: " << zone.num_layers() << std::endl;
    std::cout << "- Has elevation layer: " << zone.has_layer("elevation") << std::endl;
    
    // Save files
    std::string vector_path = "/tmp/debug_zone.geojson";
    std::string raster_path = "/tmp/debug_zone.tiff";
    
    std::filesystem::remove(vector_path);
    std::filesystem::remove(raster_path);
    
    std::cout << "\nSaving to files..." << std::endl;
    try {
        zone.toFiles(vector_path, raster_path);
        std::cout << "Save completed" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Save error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Files exist:" << std::endl;
    std::cout << "- GeoJSON: " << std::filesystem::exists(vector_path) << std::endl;
    std::cout << "- GeoTIFF: " << std::filesystem::exists(raster_path) << std::endl;
    
    if (std::filesystem::exists(vector_path)) {
        std::cout << "- GeoJSON size: " << std::filesystem::file_size(vector_path) << " bytes" << std::endl;
    }
    if (std::filesystem::exists(raster_path)) {
        std::cout << "- GeoTIFF size: " << std::filesystem::file_size(raster_path) << " bytes" << std::endl;
    }
    
    // Load back
    std::cout << "\nLoading from files..." << std::endl;
    Zone loaded_zone;
    try {
        loaded_zone = Zone::fromFiles(vector_path, raster_path);
        std::cout << "Load completed" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Load error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nAfter load:" << std::endl;
    std::cout << "- Name: " << loaded_zone.getName() << std::endl;
    std::cout << "- Type: " << loaded_zone.getType() << std::endl;
    std::cout << "- Elements: " << loaded_zone.get_elements().size() << std::endl;
    std::cout << "- Layers: " << loaded_zone.num_layers() << std::endl;
    
    std::cout << "\nLoaded zone properties:" << std::endl;
    std::cout << "- crop_type: '" << loaded_zone.getProperty("crop_type") << "'" << std::endl;
    std::cout << "- test_prop: '" << loaded_zone.getProperty("test_prop") << "'" << std::endl;
    
    std::cout << "- Has elevation layer: " << loaded_zone.has_layer("elevation") << std::endl;
    
    // Show the actual GeoJSON content
    std::cout << "\n=== GeoJSON Content ===" << std::endl;
    std::ifstream geojson_file(vector_path);
    if (geojson_file.is_open()) {
        std::string line;
        int line_count = 0;
        while (std::getline(geojson_file, line) && line_count < 20) {
            std::cout << line << std::endl;
            line_count++;
        }
        geojson_file.close();
    }
    
    return 0;
}