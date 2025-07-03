#include <iostream>
#include <random>

#include "entropy/generator.hpp"
#include "zoneout/zoneout.hpp"

// Helper function to generate noise-based grid data
concord::Grid<uint8_t> generate_noise(const concord::Size &size, entropy::NoiseGen::NoiseType noise_type,
                                      float frequency, uint8_t min_val, uint8_t max_val) {
    // Create grid with proper spatial positioning
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> grid(static_cast<size_t>(size.y), static_cast<size_t>(size.x), 1.0, true, shift);

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

    // Create an L-shaped polygon
    std::vector<concord::Point> l_points;
    l_points.emplace_back(0, 0, 0);    // Bottom-left corner
    l_points.emplace_back(100, 0, 0);  // Bottom-right of horizontal part
    l_points.emplace_back(100, 30, 0); // Top-right of horizontal part
    l_points.emplace_back(30, 30, 0);  // Inner corner of L
    l_points.emplace_back(30, 80, 0);  // Top-right of vertical part
    l_points.emplace_back(0, 80, 0);   // Top-left corner
    concord::Polygon l_boundary(l_points);

    // Create field zone - constructor automatically generates base layer with noise inside polygon
    zoneout::Zone field("L-Shaped Field", "field", l_boundary, WAGENINGEN_DATUM);

    std::cout << "Created field: " << field.getName() << std::endl;
    std::cout << "Field area: " << field.poly_data_.area() << " m²" << std::endl;
    std::cout << field.getRasterInfo() << std::endl;

    // Get grid configuration from the auto-generated base layer
    const auto &base_grid = field.grid_data_.getGrid(0).grid;

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
