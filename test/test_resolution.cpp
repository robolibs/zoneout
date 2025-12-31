#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "zoneout/zoneout.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace dp = datapod;
using namespace zoneout;

TEST_CASE("Test different resolutions with same polygon") {
    // Use fixed coordinates for testing
    dp::Geo test_datum{52.0, 5.0, 0.0};

    // Create a simple square polygon (100m x 100m)
    dp::Polygon square;
    square.vertices.push_back({0, 0, 0});
    square.vertices.push_back({100, 0, 0});
    square.vertices.push_back({100, 100, 0});
    square.vertices.push_back({0, 100, 0});

    std::vector<double> resolutions = {0.1, 0.5, 1.0, 2.0, 5.0};

    for (double res : resolutions) {
        SUBCASE(("Resolution " + std::to_string(res) + "m").c_str()) {
            // Create zone with specific resolution
            Zone zone("Test Field", "agricultural", square, test_datum, res);
            zone.set_property("resolution", std::to_string(res));

            // Save to files
            std::string output_dir = "/tmp/test_resolution_" + std::to_string(res);
            std::filesystem::remove_all(output_dir);
            zone.save(output_dir);

            // Get the grid info
            const auto &grid_data = zone.raster_data();
            if (grid_data.hasGrids()) {
                const auto &first_layer = grid_data.getGrid(0);
                const auto &grid = first_layer.grid;

                std::cout << "\n=== Resolution " << res << "m ===" << std::endl;
                std::cout << "Grid dimensions: " << grid.cols() << " x " << grid.rows() << std::endl;
                std::cout << "Grid size in meters: " << (grid.cols() * res) << " x " << (grid.rows() * res)
                          << std::endl;

                // Calculate what the GeoTIFF extent should be
                double grid_width_meters = grid.cols() * res;
                double grid_height_meters = grid.rows() * res;

                // Get the shift (center) of the grid
                auto shift = grid_data.getShift();
                std::cout << "Grid center (ENU): " << shift.point.x << ", " << shift.point.y << std::endl;

                // Convert to WGS84 to see the actual geographic extent
                dp::ENU center_enu(shift.point.x, shift.point.y, shift.point.z, test_datum);
                dp::WGS center_wgs = center_enu.toWGS();

                // Calculate corners
                dp::ENU tl_enu(shift.point.x - grid_width_meters / 2, shift.point.y + grid_height_meters / 2, 0,
                               test_datum);
                dp::ENU br_enu(shift.point.x + grid_width_meters / 2, shift.point.y - grid_height_meters / 2, 0,
                               test_datum);
                dp::WGS tl_wgs = tl_enu.toWGS();
                dp::WGS br_wgs = br_enu.toWGS();

                std::cout << std::fixed << std::setprecision(10);
                std::cout << "Center (WGS): " << center_wgs.lon << ", " << center_wgs.lat << std::endl;
                std::cout << "Top-left (WGS): " << tl_wgs.lon << ", " << tl_wgs.lat << std::endl;
                std::cout << "Bottom-right (WGS): " << br_wgs.lon << ", " << br_wgs.lat << std::endl;

                double lon_span = br_wgs.lon - tl_wgs.lon;
                double lat_span = tl_wgs.lat - br_wgs.lat;
                std::cout << "Geographic span: " << lon_span << "° lon x " << lat_span << "° lat" << std::endl;

                // Grid should encompass 100m polygon with reasonable padding
                // Due to ceil() rounding, different resolutions give slightly different effective padding
                CHECK(grid_width_meters >= 100.0);  // Must encompass at least the 100m polygon
                CHECK(grid_width_meters <= 112.0);  // Reasonable upper bound with padding
                CHECK(grid_height_meters >= 100.0); // Must encompass at least the 100m polygon
                CHECK(grid_height_meters <= 112.0); // Reasonable upper bound with padding
            }

            // Verify files exist
            CHECK(std::filesystem::exists(output_dir + "/vector.geojson"));
            CHECK(std::filesystem::exists(output_dir + "/raster.tiff"));

            // Load back and check
            Zone loaded = Zone::load(output_dir);
            CHECK(loaded.name() == "Test Field");
            CHECK(loaded.get_property("resolution") == std::to_string(res));

            INFO("Files saved to: " << output_dir);
        }
    }

    // Now let's compare the GeoJSON files - they should all be identical
    SUBCASE("Compare GeoJSON files") {
        std::string geojson_1_0 = "/tmp/test_resolution_1.000000/vector.geojson";
        std::string geojson_0_1 = "/tmp/test_resolution_0.100000/vector.geojson";

        if (std::filesystem::exists(geojson_1_0) && std::filesystem::exists(geojson_0_1)) {
            std::ifstream file1(geojson_1_0);
            std::ifstream file2(geojson_0_1);

            std::string content1((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
            std::string content2((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());

            // The UUID will be different, but coordinates should be the same
            // Check that both files contain the same WGS84 coordinates (converted from ENU)
            CHECK(content1.find("5.0014560700161015") != std::string::npos);
            CHECK(content2.find("5.0014560700161015") != std::string::npos);

            INFO("GeoJSON files should have identical polygon coordinates");
        }
    }
}