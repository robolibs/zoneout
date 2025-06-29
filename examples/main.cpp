#include <cmath>
#include <iostream>
#include <random>
#include <unordered_map>

#include "entropy/generator.hpp"
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

// Helper function to generate noise-based grid data
concord::Grid<uint8_t> generate_noise(const concord::Size &size, entropy::NoiseGen::NoiseType noise_type,
                                      float frequency, uint8_t min_val, uint8_t max_val,
                                      const concord::Datum &datum = concord::Datum{0, 0, 0}, double resolution = 1.0) {
    // Create grid with proper spatial positioning
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> grid(static_cast<size_t>(size.y), static_cast<size_t>(size.x), resolution, true, shift);

    // Configure noise generator
    entropy::NoiseGen noise;
    noise.SetNoiseType(noise_type);
    noise.SetFrequency(frequency);
    noise.SetSeed(std::random_device{}());

    // Fill grid with noise data
    for (size_t r = 0; r < grid.rows(); ++r) {
        for (size_t c = 0; c < grid.cols(); ++c) {
            // Get noise value (-1 to 1) and convert to desired range
            float noise_val = noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
            uint8_t value = static_cast<uint8_t>(min_val + (noise_val + 1.0f) * 0.5f * (max_val - min_val));
            grid.set_value(r, c, value);
        }
    }

    return grid;
}

int main() {
    std::cout << "Simple Zone Example" << std::endl;

    // Wageningen Research Labs coordinates
    const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

    // Create base elevation grid (100x50 resolution)
    auto base_elevation_grid = generate_noise(concord::Size{100, 50, 0}, entropy::NoiseGen::NoiseType_OpenSimplex2,
                                              0.01f, 0, 20, WAGENINGEN_DATUM, 1.0);

    // Create a field zone with boundary and base elevation grid
    auto field_boundary = createRectangle(0, 0, 100, 50);
    zoneout::Zone field("Wheat Field", "field", field_boundary, WAGENINGEN_DATUM, base_elevation_grid);

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

    // Generate realistic elevation data (0-20 meters) using same size as base grid
    auto elevation_grid = generate_noise(concord::Size{100, 50, 0}, entropy::NoiseGen::NoiseType_OpenSimplex2,
                                         0.01f,                         // Low frequency for gentle terrain
                                         0, 20, WAGENINGEN_DATUM, 1.0); // 0-20m range
    field.addRasterLayer(elevation_grid, "elevation", "terrain", {{"units", "meters"}, {"range", "0-20m"}});

    // Generate realistic soil moisture data (20-80%)
    auto moisture_grid = generate_noise(concord::Size{100, 50, 0}, entropy::NoiseGen::NoiseType_Perlin,
                                        0.05f,                          // Higher frequency for more variation
                                        20, 80, WAGENINGEN_DATUM, 1.0); // 20-80% range
    field.addRasterLayer(moisture_grid, "soil_moisture", "environmental",
                         {{"units", "percentage"}, {"range", "20-80%"}});

    std::cout << "Added " << field.grid_data_.gridCount() << " raster layers" << std::endl;
    std::cout << field.getRasterInfo() << std::endl;

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
