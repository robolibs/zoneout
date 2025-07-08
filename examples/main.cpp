#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "entropy/generator.hpp"
#include "geoget/geoget.hpp"
#include "zoneout/zoneout.hpp"

int main() {
    std::cout << "Simple Zone Example" << std::endl;

    // Wageningen Research Labs coordinates
    const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

    geoget::PolygonDrawer drawer(WAGENINGEN_DATUM);
    if (!drawer.start(8080)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    const auto l_boundary = drawer.get_polygons();

    // Create field zone - constructor automatically generates base layer with noise inside polygon
    zoneout::Zone field("L-Shaped Field", "field", l_boundary[0], WAGENINGEN_DATUM, 0.1);

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
    field.addRasterLayer(moisture_grid, "moisture", "environmental", {{"units", "percentage"}}, true);

    std::cout << "\nFinal result: " << field.getRasterInfo() << std::endl;

    std::cout << "\nLayer details:" << std::endl;
    std::cout << "- base_layer: Auto-generated noise only inside L-shape (from constructor)" << std::endl;
    std::cout << "- temperature_full: Full rectangular coverage (poly_cut=false)" << std::endl;
    std::cout << "- moisture_cut: Cut to L-shape only (poly_cut=true)" << std::endl;

    // Add polygon features from boundary polygons (except n=0)
    std::cout << "\nAdding polygon features..." << std::endl;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> crop_dist(1, 4);
    std::uniform_int_distribution<> priority_dist(1, 10);

    std::vector<std::string> crop_types = {"corn", "wheat", "soybean", "barley"};
    std::vector<std::string> management_types = {"organic", "conventional", "precision", "sustainable"};

    for (size_t i = 1; i < l_boundary.size(); ++i) {
        std::string crop_type = crop_types[crop_dist(gen) - 1];
        std::string management_type = management_types[crop_dist(gen) - 1];

        std::unordered_map<std::string, std::string> properties = {
            {"crop_type", crop_type},
            {"management", management_type},
            {"priority", std::to_string(priority_dist(gen))},
            {"season", "spring_2024"},
            {"area_m2", std::to_string(static_cast<int>(l_boundary[i].area()))}};

        std::string feature_name = "field_section_" + std::to_string(i);

        try {
            field.addPolygonFeature(l_boundary[i], feature_name, "agricultural", "crop_zone", properties);
            std::cout << "Added feature: " << feature_name << " (" << crop_type << ", " << management_type << ")"
                      << std::endl;
        } catch (const std::exception &e) {
            std::cout << "Failed to add feature " << feature_name << ": " << e.what() << std::endl;
        }
    }

    std::cout << "\nFinal features: " << field.getFeatureInfo() << std::endl;

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
