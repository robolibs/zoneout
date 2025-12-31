#include "zoneout/zoneout.hpp"

#include "geoget/geoget.hpp"
#include "rerun.hpp"
#include "rerun/recording_stream.hpp"
#include <entropy/generator.hpp>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace dp = datapod;

zoneout::Plot create_field(const std::string &zone_name, const std::string &crop_type,
                           const dp::Geo &datum = dp::Geo{51.98776171041831, 5.662378206146002, 0.0}) {
    zoneout::Plot plot("Wageningen Farm", "agricultural", datum);
    plot.set_property("farm_type", "research");
    plot.set_property("owner", "Wageningen Research Labs");

    typedef std::unordered_map<std::string, std::string> Props;

    geoget::PolygonDrawer drawer(datum);
    if (drawer.start(8080)) {
        const auto polygons = drawer.get_polygons();
        drawer.stop();

        if (!polygons.empty()) {
            zoneout::Zone zone(zone_name, "field", polygons[0], datum, 0.1);

            const auto &base_grid = zone.grid().get_layer(0).grid;
            auto temp_grid = base_grid;
            auto moisture_grid = base_grid;

            entropy::noise::NoiseGen temp_noise, moisture_noise;
            temp_noise.SetNoiseType(entropy::noise::NoiseGen::NoiseType_Perlin);
            temp_noise.SetFrequency(0.08f);
            temp_noise.SetSeed(std::random_device{}());

            moisture_noise.SetNoiseType(entropy::noise::NoiseGen::NoiseType_OpenSimplex2);
            moisture_noise.SetFrequency(0.05f);
            moisture_noise.SetSeed(std::random_device{}() + 100);

            for (size_t r = 0; r < temp_grid.rows; ++r) {
                for (size_t c = 0; c < temp_grid.cols; ++c) {
                    float temp_noise_val = temp_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
                    uint8_t temp_value = static_cast<uint8_t>(15 + (temp_noise_val + 1.0f) * 0.5f * (35 - 15));
                    temp_grid(r, c) = temp_value;

                    float moisture_noise_val = moisture_noise.GetNoise(static_cast<float>(r), static_cast<float>(c));
                    uint8_t moisture_value = static_cast<uint8_t>(20 + (moisture_noise_val + 1.0f) * 0.5f * (80 - 20));
                    moisture_grid(r, c) = moisture_value;
                }
            }

            zone.add_raster_layer(temp_grid, "temperature", "environmental", {{"units", "celsius"}}, true);
            zone.add_raster_layer(moisture_grid, "moisture", "environmental", {{"units", "percentage"}}, true);

            zone.set_property("crop_type", crop_type);
            zone.set_property("planting_date", "2024-04-15");
            zone.set_property("irrigation", "true");

            plot.add_zone(zone);
            std::cout << "Added zone: " << zone.name() << " (ID: " << zone.id().toString() << ")" << std::endl;

            // Add remaining polygons as features to the zone that's now in the plot
            if (plot.get_zone_count() > 0) {
                auto &plot_zone = plot.get_zones().back(); // Get the zone we just added

                for (size_t i = 1; i < polygons.size(); ++i) {
                    Props properties = {{"area_m2", std::to_string(static_cast<int>(polygons[i].area()))}};
                    std::string feature_name = zone_name + "_feature_" + std::to_string(i);
                    try {
                        plot_zone.add_polygon_feature(polygons[i], feature_name, "obstacle", "obstacle", properties);
                    } catch (const std::exception &e) {
                        std::cout << "Failed to add feature " << feature_name << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }
    return plot;
}

int main() {

    auto rec = std::make_shared<rerun::RecordingStream>("zoneout", "space");
    if (rec->connect_grpc("rerun+http://127.0.0.1:9876/proxy").is_err()) {
        std::cerr << "Failed to connect to rerun\n";
        return 1;
    }
    rec->log("", rerun::Clear::RECURSIVE);
    rec->log_with_static("", true, rerun::Clear::RECURSIVE);

    // auto farm = create_field("Pea_Field", "pea", dp::Geo{51.73019, 4.23883, 0.0});
    // farm.save("/home/bresilla/farm_plot_2");
    auto farm = zoneout::Plot::load("/home/bresilla/farm_plot_2", "Pea Farm", "agricultural");

    auto zones = farm.get_zones();
    std::cout << "Num zones: " << zones.size() << std::endl;

    auto zone0 = zones.at(0);
    auto boundary = zone0.poly().get_field_boundary();
    std::cout << "Zone 0 boundary: " << boundary.vertices.size() << " points" << std::endl;

    for (size_t i = 0; i < zones.size(); ++i) {
        zoneout::visualize::show_zone(zones.at(i), rec, zones.at(i).datum(), zones.at(i).name(), i);
    }

    return 0;
}
