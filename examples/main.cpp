#include <iostream>
#include <random>

#include "entropy/generator.hpp"
#include "geoget/geoget.hpp"
#include "zoneout/zoneout.hpp"

int main() {
    std::cout << "Simple Zone Example" << std::endl;

    // Wageningen Research Labs coordinates

    geoget::PolygonDrawer drawer;
    if (!drawer.start(8080)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    auto datum = drawer.add_datum();
    const concord::Datum WAGENINGEN_DATUM{datum.lat, datum.lon, 0.0};

    drawer.stop();
    if (!drawer.start(8081)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    const auto l_boundary = drawer.get_polygons();

    // Create field zone - constructor automatically generates base layer with noise inside polygon
    zoneout::Zone field("L-Shaped Field", "field", l_boundary[0], WAGENINGEN_DATUM, 0.1);

    const auto &base_grid = field.grid_data_.getGrid(0).grid;
    
    // If we have a second polygon, draw it with value 0.5 (128) on the base layer for coordinate testing
    if (l_boundary.size() > 1) {
        std::cout << "\nDrawing test polygon (polygon[1]) with value 0.5 on base layer..." << std::endl;
        auto &mutable_base_grid = field.grid_data_.getGrid(0).grid;
        
        // Draw the test polygon with value 128 (0.5 in normalized range)
        for (size_t r = 0; r < mutable_base_grid.rows(); ++r) {
            for (size_t c = 0; c < mutable_base_grid.cols(); ++c) {
                auto cell_center = mutable_base_grid.get_point(r, c);
                if (l_boundary[1].contains(cell_center)) {
                    mutable_base_grid.set_value(r, c, 128); // 0.5 in uint8_t range
                }
            }
        }
        std::cout << "Test polygon drawn on base layer with value 128 (0.5)" << std::endl;
    } else {
        std::cout << "\nNo second polygon drawn - only one polygon provided" << std::endl;
    }

    // Create new grids with the SAME spatial configuration as the base layer
    auto temp_grid = base_grid;     // Copy spatial properties (pose, resolution, size)
    auto moisture_grid = base_grid; // Copy spatial properties (pose, resolution, size)

    // Configure noise generators
    entropy::NoiseGen temp_noise, moisture_noise;
    temp_noise.SetNoiseType(entropy::NoiseGen::NoiseType_Perlin);
    temp_noise.SetFrequency(0.08f);
    temp_noise.SetSeed(std::random_device{}());

    moisture_noise.SetNoiseType(entropy::NoiseGen::NoiseType_OpenSimplex2);
    moisture_noise.SetFrequency(0.05f);
    moisture_noise.SetSeed(std::random_device{}());

    // Fill grids with noise data
    for (size_t r = 0; r < temp_grid.rows(); ++r) {
        for (size_t c = 0; c < temp_grid.cols(); ++c) {
            // Temperature grid (15-35 range)
            float temp_noise_val = temp_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
            uint8_t temp_value = static_cast<uint8_t>(15 + (temp_noise_val + 1.0f) * 0.5f * (35 - 15));
            temp_grid.set_value(r, c, temp_value);

            // Moisture grid (20-80 range)
            float moisture_noise_val = moisture_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
            uint8_t moisture_value = static_cast<uint8_t>(20 + (moisture_noise_val + 1.0f) * 0.5f * (80 - 20));
            moisture_grid.set_value(r, c, moisture_value);
        }
    }

    // Add first raster - WITHOUT poly_cut (full rectangular coverage)
    field.addRasterLayer(temp_grid, "temperature_full", "environmental", {{"units", "celsius"}}, false);

    // Add second raster - WITH poly_cut (zeros outside L-shape)
    field.addRasterLayer(moisture_grid, "moisture_cut", "environmental", {{"units", "percentage"}}, true);
    field.addRasterLayer(moisture_grid, "moisture", "environmental", {{"units", "percentage"}}, true);

    std::cout << "\nFinal result: " << field.getRasterInfo() << std::endl;

    std::cout << "\nLayer details:" << std::endl;
    std::cout << "- base_layer: Auto-generated noise only inside L-shape (from constructor)" << std::endl;
    std::cout << "- temperature_full: Full rectangular coverage (poly_cut=false)" << std::endl;
    std::cout << "- moisture_cut: Cut to L-shape only (poly_cut=true)" << std::endl;

    // Save to files
    field.toFiles("/tmp/a_field.geojson", "/tmp/a_field.tiff");
    std::cout << "\nSaved field to files" << std::endl;

    // Load back and verify
    auto loaded_field = zoneout::Zone::fromFiles("/tmp/a_field.geojson", "/tmp/a_field.tiff");
    std::cout << "Loaded field: " << loaded_field.getName() << std::endl;
    std::cout << "Loaded " << loaded_field.getRasterInfo() << std::endl;
    std::cout << "Area preserved: " << (field.poly_data_.area() == loaded_field.poly_data_.area() ? "Yes" : "No")
              << std::endl;

    return 0;
}
