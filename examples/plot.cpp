#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "entropy/generator.hpp"
#include "geoget/geoget.hpp"
#include "zoneout/zoneout.hpp"

void createZoneFromPolygons(zoneout::Plot &plot, const std::string &zone_name, const std::string &crop_type,
                            const concord::Datum &datum) {
    geoget::PolygonDrawer drawer(datum);
    if (drawer.start(8080)) {
        const auto polygons = drawer.get_polygons();
        drawer.stop();

        if (!polygons.empty()) {
            zoneout::Zone zone(zone_name, "field", polygons[0], datum, 0.1);

            const auto &base_grid = zone.grid_data_.getGrid(0).grid;
            auto temp_grid = base_grid;
            auto moisture_grid = base_grid;

            entropy::NoiseGen temp_noise, moisture_noise;
            temp_noise.SetNoiseType(entropy::NoiseGen::NoiseType_Perlin);
            temp_noise.SetFrequency(0.08f);
            temp_noise.SetSeed(std::random_device{}());

            moisture_noise.SetNoiseType(entropy::NoiseGen::NoiseType_OpenSimplex2);
            moisture_noise.SetFrequency(0.05f);
            moisture_noise.SetSeed(std::random_device{}() + 100);

            for (size_t r = 0; r < temp_grid.rows(); ++r) {
                for (size_t c = 0; c < temp_grid.cols(); ++c) {
                    float temp_noise_val = temp_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
                    uint8_t temp_value = static_cast<uint8_t>(15 + (temp_noise_val + 1.0f) * 0.5f * (35 - 15));
                    temp_grid.set_value(r, c, temp_value);

                    float moisture_noise_val = moisture_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
                    uint8_t moisture_value = static_cast<uint8_t>(20 + (moisture_noise_val + 1.0f) * 0.5f * (80 - 20));
                    moisture_grid.set_value(r, c, moisture_value);
                }
            }

            zone.addRasterLayer(temp_grid, "temperature", "environmental", {{"units", "celsius"}}, true);
            zone.addRasterLayer(moisture_grid, "moisture", "environmental", {{"units", "percentage"}}, true);

            zone.setProperty("crop_type", crop_type);
            zone.setProperty("planting_date", "2024-04-15");
            zone.setProperty("irrigation", "true");

            plot.addZone(zone);
            std::cout << "Added zone: " << zone.getName() << " (ID: " << zone.getId().toString() << ")" << std::endl;

            // Add remaining polygons as features to the zone that's now in the plot
            if (plot.getZoneCount() > 0) {
                auto &plot_zone = plot.getZones().back(); // Get the zone we just added

                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> crop_dist(1, 4);
                std::uniform_int_distribution<> priority_dist(1, 10);
                std::vector<std::string> crop_types = {"corn", "wheat", "soybean", "barley"};
                std::vector<std::string> management_types = {"organic", "conventional", "precision", "sustainable"};

                for (size_t i = 1; i < polygons.size(); ++i) {
                    std::string feat_crop = crop_types[crop_dist(gen) - 1];
                    std::string management = management_types[crop_dist(gen) - 1];

                    std::unordered_map<std::string, std::string> properties = {
                        {"crop_type", feat_crop},
                        {"management", management},
                        {"priority", std::to_string(priority_dist(gen))},
                        {"season", "spring_2024"},
                        {"area_m2", std::to_string(static_cast<int>(polygons[i].area()))}};

                    std::string feature_name = zone_name + "_feature_" + std::to_string(i);

                    try {
                        plot_zone.addPolygonFeature(polygons[i], feature_name, "agricultural", "crop_zone", properties);
                        std::cout << "Added feature: " << feature_name << " (" << feat_crop << ", " << management << ")"
                                  << std::endl;
                    } catch (const std::exception &e) {
                        std::cout << "Failed to add feature " << feature_name << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }
}

void save_file() {
    // std::cout << "Plot Management Example" << std::endl;
    //
    // // Wageningen Research Labs coordinates
    const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};
    //
    // // Create a Plot to manage multiple zones
    zoneout::Plot farm_plot("Wageningen Farm", "agricultural", WAGENINGEN_DATUM);
    farm_plot.setProperty("farm_type", "research");
    farm_plot.setProperty("owner", "Wageningen Research Labs");

    std::cout << "Created plot: " << farm_plot.getName() << " (ID: " << farm_plot.getId().toString() << ")"
              << std::endl;

    // Create first zone
    createZoneFromPolygons(farm_plot, "North Field", "corn", WAGENINGEN_DATUM);

    std::cout << "\nPlot Summary:" << std::endl;
    std::cout << "- Total zones: " << farm_plot.getZoneCount() << std::endl;
    std::cout << "- Plot type: " << farm_plot.getType() << std::endl;
    std::cout << "- Owner: " << farm_plot.getProperty("owner") << std::endl;

    // Create second zone
    std::cout << "\nCreate second zone..." << std::endl;
    createZoneFromPolygons(farm_plot, "South Field", "wheat", WAGENINGEN_DATUM);

    if (farm_plot.getZoneCount() > 0) {
        std::cout << "\nFinal features: " << farm_plot.getZones()[0].getFeatureInfo() << std::endl;
    } else {
        std::cout << "\nNo zones in plot to check features." << std::endl;
    }

    // Save to files
    farm_plot.save("/tmp/farm_plot");
    std::cout << "\nSaved farm plot to files" << std::endl;

    // Test loading the saved files
    std::cout << "\nTesting load functionality..." << std::endl;
    auto loaded_plot =
        zoneout::Plot::load("/tmp/farm_plot", "Loaded Wageningen Farm", "agricultural", WAGENINGEN_DATUM);

    std::cout << "Loaded plot: " << loaded_plot.getName() << " (ID: " << loaded_plot.getId().toString() << ")"
              << std::endl;
    std::cout << "- Total zones loaded: " << loaded_plot.getZoneCount() << std::endl;
    std::cout << "- Plot type: " << loaded_plot.getType() << std::endl;

    if (loaded_plot.getZoneCount() > 0) {
        for (size_t i = 0; i < loaded_plot.getZoneCount(); ++i) {
            std::cout << "- Zone " << (i + 1) << ": " << loaded_plot.getZones()[i].getName()
                      << " (ID: " << loaded_plot.getZones()[i].getId().toString() << ")" << std::endl;
            std::cout << "- Zone " << (i + 1) << " features: " << loaded_plot.getZones()[i].getFeatureInfo()
                      << std::endl;
            std::cout << "- Zone " << (i + 1) << " raster info: " << loaded_plot.getZones()[i].getRasterInfo()
                      << std::endl;
            // print the zone's polygon features UUIDs
            const auto &polygon_elements = loaded_plot.getZones()[i].poly_data_.getPolygonElements();
            for (size_t j = 0; j < polygon_elements.size(); ++j) {
                std::cout << "- Zone " << (i + 1) << " polygon feature " << (j + 1) << ": "
                          << polygon_elements[j].uuid.toString() << " (" << polygon_elements[j].name << ")"
                          << std::endl;
            }
        }
    }
}

void load_tar() {
    const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

    auto farm_plot = zoneout::Plot::load("/tmp/farm_plot", "Loaded Wageningen Farm", "agricultural", WAGENINGEN_DATUM);

    // Save to tar file
    farm_plot.save_tar("/tmp/farm_plot.tar");
    std::cout << "\nSaved farm plot to tar file" << std::endl;

    // Test loading the tar file
    std::cout << "\nTesting tar load functionality..." << std::endl;
    auto loaded_plot_tar =
        zoneout::Plot::load_tar("/tmp/farm_plot.tar", "Loaded Wageningen Farm (tar)", "agricultural", WAGENINGEN_DATUM);

    std::cout << "Loaded plot from tar: " << loaded_plot_tar.getName() << " (ID: " << loaded_plot_tar.getId().toString()
              << ")" << std::endl;
    std::cout << "- Total zones loaded from tar: " << loaded_plot_tar.getZoneCount() << std::endl;
    std::cout << "- Plot type: " << loaded_plot_tar.getType() << std::endl;

    if (loaded_plot_tar.getZoneCount() > 0) {
        for (size_t i = 0; i < loaded_plot_tar.getZoneCount(); ++i) {
            std::cout << "- Zone " << (i + 1) << ": " << loaded_plot_tar.getZones()[i].getName()
                      << " (ID: " << loaded_plot_tar.getZones()[i].getId().toString() << ")" << std::endl;
            std::cout << "- Zone " << (i + 1) << " features: " << loaded_plot_tar.getZones()[i].getFeatureInfo()
                      << std::endl;
            std::cout << "- Zone " << (i + 1) << " raster info: " << loaded_plot_tar.getZones()[i].getRasterInfo()
                      << std::endl;
            // print the zone's polygon features UUIDs
            const auto &polygon_elements = loaded_plot_tar.getZones()[i].poly_data_.getPolygonElements();
            for (size_t j = 0; j < polygon_elements.size(); ++j) {
                std::cout << "- Zone " << (i + 1) << " polygon feature " << (j + 1) << ": "
                          << polygon_elements[j].uuid.toString() << " (" << polygon_elements[j].name << ")"
                          << std::endl;
            }
        }
    }
}

int main() {
    save_file();
    load_tar();
    return 0;
}
