#include <iostream>
#include <unordered_map>

#include "zoneout/zoneout.hpp"

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
    std::cout << "Simple Zone Example" << std::endl;

    // Create a field zone with boundary
    auto field_boundary = createRectangle(0, 0, 100, 50);
    zoneout::Zone field("Wheat Field", "field", field_boundary);

    std::cout << "Created field: " << field.getName() << std::endl;
    std::cout << "Field area: " << field.poly_data_.area() << " m²" << std::endl;

    // Add field properties
    field.setProperty("crop_type", "wheat");
    field.setProperty("planting_date", "2024-03-15");

    // Add structured polygon elements with UUIDs
    auto parking_area = createRectangle(110, 10, 20, 15);
    field.poly_data_.addPolygonElement(zoneout::generateUUID(), "main_parking", "parking_space", "vehicle", 
                                      parking_area, {{"capacity", "5_vehicles"}});

    auto storage_building = createRectangle(110, 30, 25, 20);
    field.poly_data_.addPolygonElement(zoneout::generateUUID(), "equipment_storage", "storage_facility", "building",
                                      storage_building, {{"capacity", "200_tons"}});

    // Add structured point element with UUID
    field.poly_data_.addPointElement(zoneout::generateUUID(), "irrigation_hub", "equipment_station", "water",
                                    concord::Point(50, 25, 0), {{"flow_rate", "150L_per_min"}});

    std::cout << "Added " << field.poly_data_.elementCount() << " elements" << std::endl;

    // Add elevation layer
    field.grid_data_.addGrid(20, 10, "elevation", "terrain", {{"units", "meters"}});

    // Add soil moisture layer
    field.grid_data_.addGrid(20, 10, "soil_moisture", "environmental", {{"units", "percentage"}});

    std::cout << "Added " << field.grid_data_.gridCount() << " raster layers" << std::endl;

    // Test point containment
    concord::Point test_point(50, 25, 0);
    std::cout << "Point (50,25) in field: " << (field.poly_data_.contains(test_point) ? "Yes" : "No") << std::endl;

    // Save to files
    try {
        field.toFiles("/tmp/simple_field.geojson", "/tmp/simple_field.tiff");
        std::cout << "Saved field to files" << std::endl;

        // Load back
        auto loaded_field = zoneout::Zone::fromFiles("/tmp/simple_field.geojson", "/tmp/simple_field.tiff");
        std::cout << "Loaded field: " << loaded_field.getName() << std::endl;
        std::cout << "Elements: " << loaded_field.poly_data_.elementCount() << std::endl;
        std::cout << "Layers: " << loaded_field.grid_data_.gridCount() << std::endl;

    } catch (const std::exception &e) {
        std::cout << "File I/O error: " << e.what() << std::endl;
    }

    std::cout << "Example completed!" << std::endl;
    return 0;
}
