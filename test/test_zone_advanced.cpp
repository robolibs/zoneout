#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

#include "zoneout/zoneout.hpp"

using namespace zoneout;

// Wageningen Research Labs coordinates
const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

// Helper function to create a rectangular polygon
concord::Polygon createRectangle(double x, double y, double width, double height) {
    std::vector<concord::Point> points;
    points.emplace_back(x, y, 0.0);
    points.emplace_back(x + width, y, 0.0);
    points.emplace_back(x + width, y + height, 0.0);
    points.emplace_back(x, y + height, 0.0);
    return concord::Polygon(points);
}

TEST_CASE("Zone field elements management") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

    auto boundary = createRectangle(0, 0, 200, 100);
    Zone zone("Field Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Add irrigation lines") {
        std::vector<concord::Point> line_points;
        line_points.emplace_back(10, 50, 0);
        line_points.emplace_back(190, 50, 0);

        std::unordered_map<std::string, std::string> props;
        props["flow_rate"] = "50L/min";
        props["pressure"] = "2.5bar";

        zone.poly().addElement(line_points, "irrigation_line", props);

        auto irrigation_lines = zone.poly().getElementsByType("irrigation_line");
        CHECK(irrigation_lines.size() == 1);

        auto all_elements = zone.poly().elementCount();
        CHECK(all_elements == 1);

        auto irrigation_only = zone.poly().getElementsByType("irrigation_line");
        CHECK(irrigation_only.size() == 1);

        auto crop_rows = zone.poly().getElementsByType("crop_row");
        CHECK(crop_rows.size() == 0);
    }

    SUBCASE("Add crop rows") {
        for (int i = 0; i < 5; ++i) {
            std::vector<concord::Point> row_points;
            double y = 10.0 + i * 15.0;
            row_points.emplace_back(5, y, 0);
            row_points.emplace_back(195, y, 0);

            std::unordered_map<std::string, std::string> props;
            props["row_number"] = std::to_string(i + 1);
            props["crop_type"] = "wheat";
            props["planting_date"] = "2024-03-15";

            zone.poly().addElement(row_points, "crop_row", props);
        }

        auto crop_rows = zone.poly().getElementsByType("crop_row");
        CHECK(crop_rows.size() == 5);

        auto all_elements = zone.poly().elementCount();
        CHECK(all_elements == 5);
    }

    SUBCASE("Add obstacles") {
        // Add a rectangular obstacle (e.g., building)
        auto obstacle_boundary = createRectangle(50, 25, 20, 10);

        std::unordered_map<std::string, std::string> props;
        props["type"] = "building";
        props["height"] = "5.0m";
        props["material"] = "concrete";

        zone.poly().addElement(obstacle_boundary, "obstacle", props);

        auto obstacles = zone.poly().getElementsByType("obstacle");
        CHECK(obstacles.size() == 1);
    }

    SUBCASE("Add access paths") {
        std::vector<concord::Point> path_points;
        path_points.emplace_back(0, 0, 0);
        path_points.emplace_back(50, 25, 0);
        path_points.emplace_back(100, 50, 0);
        path_points.emplace_back(200, 100, 0);

        std::unordered_map<std::string, std::string> props;
        props["width"] = "3.0m";
        props["surface"] = "gravel";
        props["max_speed"] = "15km/h";

        zone.poly().addElement(path_points, "access_path", props);

        auto access_paths = zone.poly().getElementsByType("access_path");
        CHECK(access_paths.size() == 1);
    }

    SUBCASE("Mixed field elements") {
        // Add multiple types
        std::vector<concord::Point> line_points;
        line_points.emplace_back(10, 30, 0);
        line_points.emplace_back(190, 30, 0);
        zone.poly().addElement(line_points, "irrigation_line");

        std::vector<concord::Point> row_points;
        row_points.emplace_back(5, 70, 0);
        row_points.emplace_back(195, 70, 0);
        zone.poly().addElement(row_points, "crop_row");

        auto obstacle = createRectangle(100, 10, 10, 10);
        zone.poly().addElement(obstacle, "obstacle");

        // Check totals
        CHECK(zone.poly().getElementsByType("irrigation_line").size() == 1);
        CHECK(zone.poly().getElementsByType("crop_row").size() == 1);
        CHECK(zone.poly().getElementsByType("obstacle").size() == 1);
        CHECK(zone.poly().elementCount() == 3);
    }
}

TEST_CASE("Zone raster layers management") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Raster Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Add elevation layer") {
        concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});

        // Create elevation gradient
        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                uint8_t elevation = static_cast<uint8_t>(100 + r * 2 + c);
                elevation_grid.set_value(r, c, elevation);
            }
        }

        zone.raster_data().addGrid(elevation_grid.cols(), elevation_grid.rows(), "elevation", "terrain",
                                   {{"units", "meters"}});

        // Copy the grid data
        auto &raster_grid = zone.raster_data().getGrid("elevation").grid;
        for (size_t r = 0; r < elevation_grid.rows(); ++r) {
            for (size_t c = 0; c < elevation_grid.cols(); ++c) {
                auto cell = elevation_grid(r, c);
                raster_grid.set_value(r, c, cell);
            }
        }

        CHECK(zone.raster_data().gridCount() == 2);           // Base grid + elevation layer
        CHECK(zone.raster_data().getGridNames().size() == 2); // Base grid + elevation layer
        // Check that elevation layer is present (might not be first due to base grid)
        auto grid_names = zone.raster_data().getGridNames();
        CHECK(std::find(grid_names.begin(), grid_names.end(), "elevation") != grid_names.end());

        const auto &layer = zone.raster_data().getGrid("elevation");
        CHECK(layer.name == "elevation");
        CHECK(layer.type == "terrain");
    }

    SUBCASE("Add soil moisture layer") {
        concord::Grid<uint8_t> moisture_grid(8, 16, 6.25, true, concord::Pose{});

        // Create moisture pattern
        for (size_t r = 0; r < 8; ++r) {
            for (size_t c = 0; c < 16; ++c) {
                uint8_t moisture = static_cast<uint8_t>(30 + (r + c) % 40);
                moisture_grid.set_value(r, c, moisture);
            }
        }

        zone.raster_data().addGrid(moisture_grid.cols(), moisture_grid.rows(), "soil_moisture", "environmental",
                                   {{"units", "percentage"}});

        // Copy the grid data
        auto &raster_grid = zone.raster_data().getGrid("soil_moisture").grid;
        for (size_t r = 0; r < moisture_grid.rows(); ++r) {
            for (size_t c = 0; c < moisture_grid.cols(); ++c) {
                auto cell = moisture_grid(r, c);
                raster_grid.set_value(r, c, cell);
            }
        }

        CHECK(zone.raster_data().gridCount() == 2); // Base grid + soil moisture layer
        auto grid_names = zone.raster_data().getGridNames();
        CHECK(std::find(grid_names.begin(), grid_names.end(), "soil_moisture") != grid_names.end());
    }

    SUBCASE("Add crop health layer") {
        concord::Grid<uint8_t> health_grid(12, 24, 4.16, true, concord::Pose{});

        // Create NDVI-like pattern
        for (size_t r = 0; r < 12; ++r) {
            for (size_t c = 0; c < 24; ++c) {
                uint8_t ndvi = static_cast<uint8_t>(128 + (r * c) % 127);
                health_grid.set_value(r, c, ndvi);
            }
        }

        zone.raster_data().addGrid(health_grid.cols(), health_grid.rows(), "crop_health", "vegetation",
                                   {{"units", "NDVI"}});

        // Copy the grid data
        auto &raster_grid = zone.raster_data().getGrid("crop_health").grid;
        for (size_t r = 0; r < health_grid.rows(); ++r) {
            for (size_t c = 0; c < health_grid.cols(); ++c) {
                auto cell = health_grid(r, c);
                raster_grid.set_value(r, c, cell);
            }
        }

        CHECK(zone.raster_data().gridCount() == 2); // Base grid + crop health layer
        auto grid_names = zone.raster_data().getGridNames();
        CHECK(std::find(grid_names.begin(), grid_names.end(), "crop_health") != grid_names.end());
    }

    SUBCASE("Multiple raster layers") {
        // Add all three types
        concord::Grid<uint8_t> grid1(10, 20, 5.0, true, concord::Pose{});
        concord::Grid<uint8_t> grid2(10, 20, 5.0, true, concord::Pose{});
        concord::Grid<uint8_t> grid3(10, 20, 5.0, true, concord::Pose{});

        // Fill grids with test data
        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                grid1.set_value(r, c, static_cast<uint8_t>(100 + r + c));
                grid2.set_value(r, c, static_cast<uint8_t>(50 + r * 2));
                grid3.set_value(r, c, static_cast<uint8_t>(200 - c));
            }
        }

        zone.raster_data().addGrid(grid1.cols(), grid1.rows(), "elevation", "terrain", {{"units", "meters"}});
        zone.raster_data().addGrid(grid2.cols(), grid2.rows(), "soil_moisture", "environmental",
                                   {{"units", "percentage"}});
        zone.raster_data().addGrid(grid3.cols(), grid3.rows(), "crop_health", "vegetation", {{"units", "NDVI"}});

        // Copy the grid data
        auto &raster_grid1 = zone.raster_data().getGrid("elevation").grid;
        auto &raster_grid2 = zone.raster_data().getGrid("soil_moisture").grid;
        auto &raster_grid3 = zone.raster_data().getGrid("crop_health").grid;

        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                raster_grid1.set_value(r, c, static_cast<uint8_t>(100 + r + c));
                raster_grid2.set_value(r, c, static_cast<uint8_t>(50 + r * 2));
                raster_grid3.set_value(r, c, static_cast<uint8_t>(200 - c));
            }
        }

        CHECK(zone.raster_data().gridCount() == 4); // Base grid + 3 additional layers
        auto grid_names = zone.raster_data().getGridNames();
        CHECK(grid_names.size() == 4); // Base grid + 3 additional layers
        CHECK(std::find(grid_names.begin(), grid_names.end(), "elevation") != grid_names.end());
        CHECK(std::find(grid_names.begin(), grid_names.end(), "soil_moisture") != grid_names.end());
        CHECK(std::find(grid_names.begin(), grid_names.end(), "crop_health") != grid_names.end());
    }

    SUBCASE("Custom raster layer") {
        concord::Grid<uint8_t> custom_grid(5, 10, 10.0, true, concord::Pose{});

        // Fill with custom data
        for (size_t r = 0; r < 5; ++r) {
            for (size_t c = 0; c < 10; ++c) {
                custom_grid.set_value(r, c, static_cast<uint8_t>(r * 10 + c));
            }
        }

        std::unordered_map<std::string, std::string> props;
        props["sensor_type"] = "infrared";
        props["measurement_date"] = "2024-06-15";
        props["weather_conditions"] = "sunny";

        zone.raster_data().addGrid(custom_grid.cols(), custom_grid.rows(), "temperature", "thermal", props);

        // Copy the grid data
        auto &raster_grid = zone.raster_data().getGrid("temperature").grid;
        for (size_t r = 0; r < custom_grid.rows(); ++r) {
            for (size_t c = 0; c < custom_grid.cols(); ++c) {
                auto cell = custom_grid(r, c);
                raster_grid.set_value(r, c, cell);
            }
        }

        CHECK(zone.raster_data().gridCount() == 2); // Base grid + temperature layer
        auto grid_names = zone.raster_data().getGridNames();
        CHECK(std::find(grid_names.begin(), grid_names.end(), "temperature") != grid_names.end());
    }
}

TEST_CASE("Zone raster sampling") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Sampling Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    // Create elevation grid with known pattern
    concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 10; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            uint8_t elevation = static_cast<uint8_t>(100 + r + c);
            elevation_grid.set_value(r, c, elevation);
        }
    }
    zone.raster_data().addGrid(elevation_grid.cols(), elevation_grid.rows(), "elevation", "terrain",
                               {{"units", "meters"}});

    // Copy the grid data
    auto &raster_grid = zone.raster_data().getGrid("elevation").grid;
    for (size_t r = 0; r < elevation_grid.rows(); ++r) {
        for (size_t c = 0; c < elevation_grid.cols(); ++c) {
            auto cell = elevation_grid(r, c);
            raster_grid.set_value(r, c, cell);
        }
    }

    SUBCASE("Sample at specific points") {
        // Direct access to raster grid for sampling
        const auto &layer = zone.raster_data().getGrid("elevation");

        // Sample at grid cell (2, 2) - should have value 100 + 2 + 2 = 104
        auto cell = layer.grid(2, 2);
        CHECK(cell == 104);

        // Sample at grid cell (5, 10) - should have value 100 + 5 + 10 = 115
        auto cell2 = layer.grid(5, 10);
        CHECK(cell2 == 115);
    }

    SUBCASE("Sample at grid corners") {
        const auto &layer = zone.raster_data().getGrid("elevation");

        // Sample at corner (0, 0) - should have value 100 + 0 + 0 = 100
        auto corner = layer.grid(0, 0);
        CHECK(corner == 100);

        // Sample at opposite corner (9, 19) - should have value 100 + 9 + 19 = 128
        auto far_corner = layer.grid(9, 19);
        CHECK(far_corner == 128);
    }
}

TEST_CASE("Zone geometric operations") {
    SUBCASE("Area and perimeter calculations") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

        // Rectangle: 100m x 50m = 5000 m²
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Geometry Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

        CHECK(zone.poly().area() == doctest::Approx(5000.0));
        CHECK(zone.poly().perimeter() == doctest::Approx(300.0)); // 2 * (100 + 50)
    }

    SUBCASE("Point containment") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

        auto boundary = createRectangle(10, 10, 80, 60);
        Zone zone("Containment Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

        // Points inside
        CHECK(zone.poly().contains(concord::Point(50, 40, 0)));
        CHECK(zone.poly().contains(concord::Point(20, 20, 0)));
        CHECK(zone.poly().contains(concord::Point(80, 60, 0)));

        // Points outside
        CHECK(!zone.poly().contains(concord::Point(5, 5, 0)));
        CHECK(!zone.poly().contains(concord::Point(100, 100, 0)));
        CHECK(!zone.poly().contains(concord::Point(50, 5, 0)));

        // Points on boundary (behavior may vary)
        zone.poly().contains(concord::Point(10, 40, 0)); // Left edge
        zone.poly().contains(concord::Point(90, 40, 0)); // Right edge
    }

    SUBCASE("Complex polygon") {
        // Create L-shaped polygon
        std::vector<concord::Point> l_points;
        l_points.emplace_back(0, 0, 0);
        l_points.emplace_back(60, 0, 0);
        l_points.emplace_back(60, 30, 0);
        l_points.emplace_back(30, 30, 0);
        l_points.emplace_back(30, 60, 0);
        l_points.emplace_back(0, 60, 0);

        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

        concord::Polygon l_boundary(l_points);
        Zone l_zone("L-Shape Zone", "field", l_boundary, base_grid, WAGENINGEN_DATUM);

        // Points in different parts of the L
        CHECK(l_zone.poly().contains(concord::Point(15, 15, 0))); // Bottom part
        CHECK(l_zone.poly().contains(concord::Point(15, 45, 0))); // Left part
        CHECK(l_zone.poly().contains(concord::Point(45, 15, 0))); // Right part

        // Point in the "notch" of the L
        CHECK(!l_zone.poly().contains(concord::Point(45, 45, 0)));

        // Area should be: 60*30 + 30*30 = 1800 + 900 = 2700
        CHECK(l_zone.poly().area() == doctest::Approx(2700.0));
    }
}

// TEST_CASE("Zone ownership management") {
//     Zone zone("Ownership Zone", "field");
//
//     SUBCASE("Initial state") {
//         // CHECK(!zone.hasOwner()); // hasOwner removed
//         // CHECK(zone.getOwnerRobot() // getOwnerRobot removed.isNull());
//     }
//
//     SUBCASE("Set and change owner") {
//         auto robot1 = generateUUID();
//         auto robot2 = generateUUID();
//
//         // Set first owner
//         zone.// setOwnerRobot removed: //(robot1);
//         // CHECK(!zone.hasOwner()); // hasOwner removed
//         // CHECK(zone.getOwnerRobot() // getOwnerRobot removed == robot1);
//         // CHECK(zone.getOwnerRobot() // getOwnerRobot removed != robot2);
//
//         // Change owner
//         zone.// setOwnerRobot removed: //(robot2);
//         // CHECK(!zone.hasOwner()); // hasOwner removed
//         // CHECK(zone.getOwnerRobot() // getOwnerRobot removed == robot2);
//         // CHECK(zone.getOwnerRobot() // getOwnerRobot removed != robot1);
//
//         // Release ownership
//         zone.releaseOwnership();
//         // CHECK(!zone.hasOwner()); // hasOwner removed
//         // CHECK(zone.getOwnerRobot() // getOwnerRobot removed.isNull());
//     }
// }
//
TEST_CASE("Zone validation rules") {
    SUBCASE("Valid zones") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

        auto boundary = createRectangle(0, 0, 100, 50);
        Zone valid_zone("Valid Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(valid_zone.is_valid());

        // Zone with just name and boundary is valid
        Zone minimal_zone("Minimal", "other", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(minimal_zone.is_valid());
    }

    SUBCASE("Invalid zones") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

        // No boundary
        concord::Polygon default_boundary;
        Zone no_boundary("No Boundary", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!no_boundary.is_valid());

        // Empty name
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone empty_name("", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!empty_name.is_valid());

        // Both empty name and no boundary
        Zone completely_invalid("", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!completely_invalid.is_valid());
    }
}

TEST_CASE("Zone file I/O operations") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("File I/O Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);

    // Add some data to make it interesting
    concord::Grid<uint8_t> elevation_grid(5, 10, 10.0, true, concord::Pose{});
    for (size_t r = 0; r < 5; ++r) {
        for (size_t c = 0; c < 10; ++c) {
            elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
        }
    }
    zone.raster_data().addGrid(elevation_grid.cols(), elevation_grid.rows(), "elevation", "terrain",
                               {{"units", "meters"}});

    // Copy the grid data
    auto &raster_grid = zone.raster_data().getGrid("elevation").grid;
    for (size_t r = 0; r < elevation_grid.rows(); ++r) {
        for (size_t c = 0; c < elevation_grid.cols(); ++c) {
            auto cell = elevation_grid(r, c);
            raster_grid.set_value(r, c, cell);
        }
    }

    // Add field elements
    std::vector<concord::Point> row_points;
    row_points.emplace_back(10, 25, 0);
    row_points.emplace_back(90, 25, 0);
    zone.poly().addElement(row_points, "crop_row");

    SUBCASE("Save and load files") {
        const std::string vector_path = "/tmp/zoneout_test_zone.geojson";
        const std::string raster_path = "/tmp/zoneout_test_zone.tiff";

        // Save zone
        zone.to_files(vector_path, raster_path);

        // Load zone back
        auto loaded_zone = Zone::from_files(vector_path, raster_path);

        // Basic properties should be preserved
        CHECK(loaded_zone.name() == "File I/O Zone");
        CHECK(loaded_zone.type() == "field");

        // Note: Detailed validation depends on actual geoson/geotiv implementation
        // This test mainly ensures the methods don't crash
    }
}

TEST_CASE("Zone property edge cases") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

    concord::Polygon default_boundary;
    Zone zone("Edge Case Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);

    SUBCASE("Property overwrites") {
        zone.set_property("test_key", "value1");
        CHECK(zone.get_property("test_key") == "value1");

        zone.set_property("test_key", "value2");
        CHECK(zone.get_property("test_key") == "value2");
    }

    SUBCASE("Empty property values") {
        zone.set_property("empty_key", "");
        CHECK(zone.get_property("empty_key") == "");
        CHECK(zone.get_property("empty_key", "default") == "");
    }

    SUBCASE("Special characters in properties") {
        zone.set_property("special", "value with spaces and symbols!@#$%");
        CHECK(zone.get_property("special") == "value with spaces and symbols!@#$%");

        zone.set_property("unicode", "café naïve résumé");
        CHECK(zone.get_property("unicode") == "café naïve résumé");
    }

    SUBCASE("Large number of properties") {
        // Add many properties
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i * 2);
            zone.set_property(key, value);
        }

        // Verify they're all there
        auto properties = zone.properties();
        CHECK(properties.size() == 1000);

        // Check specific values
        CHECK(zone.get_property("key_42") == "value_84");
        CHECK(zone.get_property("key_999") == "value_1998");
    }
}
