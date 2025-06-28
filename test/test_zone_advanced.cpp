#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <chrono>
#include <thread>

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

TEST_CASE("Zone field elements management") {
    auto boundary = createRectangle(0, 0, 200, 100);
    Zone zone("Field Zone", "field", boundary);
    
    SUBCASE("Add irrigation lines") {
        std::vector<concord::Point> line_points;
        line_points.emplace_back(10, 50, 0);
        line_points.emplace_back(190, 50, 0);
        concord::Path irrigation_line(line_points);
        
        std::unordered_map<std::string, std::string> props;
        props["flow_rate"] = "50L/min";
        props["pressure"] = "2.5bar";
        
        zone.addIrrigationLine(irrigation_line, props);
        
        auto irrigation_lines = zone.getIrrigationLines();
        CHECK(irrigation_lines.size() == 1);
        
        auto all_elements = zone.getFieldElements();
        CHECK(all_elements.size() == 1);
        
        auto irrigation_only = zone.getFieldElements("irrigation_line");
        CHECK(irrigation_only.size() == 1);
        
        auto crop_rows = zone.getFieldElements("crop_row");
        CHECK(crop_rows.size() == 0);
    }
    
    SUBCASE("Add crop rows") {
        for (int i = 0; i < 5; ++i) {
            std::vector<concord::Point> row_points;
            double y = 10.0 + i * 15.0;
            row_points.emplace_back(5, y, 0);
            row_points.emplace_back(195, y, 0);
            concord::Path crop_row(row_points);
            
            std::unordered_map<std::string, std::string> props;
            props["row_number"] = std::to_string(i + 1);
            props["crop_type"] = "wheat";
            props["planting_date"] = "2024-03-15";
            
            zone.addCropRow(crop_row, props);
        }
        
        auto crop_rows = zone.getCropRows();
        CHECK(crop_rows.size() == 5);
        
        auto all_elements = zone.getFieldElements();
        CHECK(all_elements.size() == 5);
    }
    
    SUBCASE("Add obstacles") {
        // Add a rectangular obstacle (e.g., building)
        auto obstacle_boundary = createRectangle(50, 25, 20, 10);
        
        std::unordered_map<std::string, std::string> props;
        props["type"] = "building";
        props["height"] = "5.0m";
        props["material"] = "concrete";
        
        zone.addObstacle(obstacle_boundary, props);
        
        auto obstacles = zone.getObstacles();
        CHECK(obstacles.size() == 1);
    }
    
    SUBCASE("Add access paths") {
        std::vector<concord::Point> path_points;
        path_points.emplace_back(0, 0, 0);
        path_points.emplace_back(50, 25, 0);
        path_points.emplace_back(100, 50, 0);
        path_points.emplace_back(200, 100, 0);
        concord::Path access_path(path_points);
        
        std::unordered_map<std::string, std::string> props;
        props["width"] = "3.0m";
        props["surface"] = "gravel";
        props["max_speed"] = "15km/h";
        
        zone.addAccessPath(access_path, props);
        
        auto access_paths = zone.getAccessPaths();
        CHECK(access_paths.size() == 1);
    }
    
    SUBCASE("Mixed field elements") {
        // Add multiple types
        std::vector<concord::Point> line_points;
        line_points.emplace_back(10, 30, 0);
        line_points.emplace_back(190, 30, 0);
        zone.addIrrigationLine(concord::Path(line_points));
        
        std::vector<concord::Point> row_points;
        row_points.emplace_back(5, 70, 0);
        row_points.emplace_back(195, 70, 0);
        zone.addCropRow(concord::Path(row_points));
        
        auto obstacle = createRectangle(100, 10, 10, 10);
        zone.addObstacle(obstacle);
        
        // Check totals
        CHECK(zone.getIrrigationLines().size() == 1);
        CHECK(zone.getCropRows().size() == 1);
        CHECK(zone.getObstacles().size() == 1);
        CHECK(zone.getFieldElements().size() == 3);
    }
}

TEST_CASE("Zone raster layers management") {
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Raster Zone", "field", boundary);
    
    SUBCASE("Add elevation layer") {
        concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
        
        // Create elevation gradient
        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                uint8_t elevation = static_cast<uint8_t>(100 + r * 2 + c);
                elevation_grid.set_value(r, c, elevation);
            }
        }
        
        zone.addElevationLayer(elevation_grid, "meters");
        
        CHECK(zone.numRasterLayers() == 1);
        CHECK(zone.hasRasterLayer("elevation"));
        
        auto layer_names = zone.getRasterLayerNames();
        CHECK(layer_names.size() == 1);
        CHECK(layer_names[0] == "elevation");
        
        auto layer = zone.getRasterLayer("elevation");
        CHECK(layer != nullptr);
        
        auto non_existent = zone.getRasterLayer("non_existent");
        CHECK(non_existent == nullptr);
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
        
        zone.addSoilMoistureLayer(moisture_grid, "percentage");
        
        CHECK(zone.numRasterLayers() == 1);
        CHECK(zone.hasRasterLayer("soil_moisture"));
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
        
        zone.addCropHealthLayer(health_grid, "NDVI");
        
        CHECK(zone.numRasterLayers() == 1);
        CHECK(zone.hasRasterLayer("crop_health"));
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
        
        zone.addElevationLayer(grid1, "meters");
        zone.addSoilMoistureLayer(grid2, "percentage");
        zone.addCropHealthLayer(grid3, "NDVI");
        
        CHECK(zone.numRasterLayers() == 3);
        CHECK(zone.hasRasterLayer("elevation"));
        CHECK(zone.hasRasterLayer("soil_moisture"));
        CHECK(zone.hasRasterLayer("crop_health"));
        
        auto all_layers = zone.getRasterLayerNames();
        CHECK(all_layers.size() == 3);
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
        
        zone.addRasterLayer("temperature", "thermal", custom_grid, props);
        
        CHECK(zone.numRasterLayers() == 1);
        CHECK(zone.hasRasterLayer("temperature"));
    }
}

TEST_CASE("Zone raster sampling") {
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Sampling Zone", "field", boundary);
    
    // Create elevation grid with known pattern
    concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 10; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            uint8_t elevation = static_cast<uint8_t>(100 + r + c);
            elevation_grid.set_value(r, c, elevation);
        }
    }
    zone.addElevationLayer(elevation_grid, "meters");
    
    SUBCASE("Sample at specific points") {
        // Sample at various points
        auto sample1 = zone.sampleRasterAt("elevation", concord::Point(12.5, 12.5, 0));
        CHECK(sample1.has_value());
        
        auto sample2 = zone.sampleRasterAt("elevation", concord::Point(50, 25, 0));
        CHECK(sample2.has_value());
        
        // Sample outside zone boundary (should still work if within raster grid)
        auto sample3 = zone.sampleRasterAt("elevation", concord::Point(-10, -10, 0));
        // May or may not have value depending on grid bounds
        
        // Sample non-existent layer
        auto sample4 = zone.sampleRasterAt("non_existent", concord::Point(25, 25, 0));
        CHECK(!sample4.has_value());
    }
    
    SUBCASE("Sample at grid corners") {
        // Sample at exact grid positions
        auto corner_sample = zone.sampleRasterAt("elevation", concord::Point(0, 0, 0));
        // Should return a value (exact behavior depends on grid implementation)
        
        auto center_sample = zone.sampleRasterAt("elevation", concord::Point(50, 25, 0));
        CHECK(center_sample.has_value());
    }
}

TEST_CASE("Zone geometric operations") {
    SUBCASE("Area and perimeter calculations") {
        // Rectangle: 100m x 50m = 5000 m²
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Geometry Zone", "field", boundary);
        
        CHECK(zone.area() == doctest::Approx(5000.0));
        CHECK(zone.perimeter() == doctest::Approx(300.0)); // 2 * (100 + 50)
    }
    
    SUBCASE("Point containment") {
        auto boundary = createRectangle(10, 10, 80, 60);
        Zone zone("Containment Zone", "field", boundary);
        
        // Points inside
        CHECK(zone.contains(concord::Point(50, 40, 0)));
        CHECK(zone.contains(concord::Point(20, 20, 0)));
        CHECK(zone.contains(concord::Point(80, 60, 0)));
        
        // Points outside
        CHECK(!zone.contains(concord::Point(5, 5, 0)));
        CHECK(!zone.contains(concord::Point(100, 100, 0)));
        CHECK(!zone.contains(concord::Point(50, 5, 0)));
        
        // Points on boundary (behavior may vary)
        zone.contains(concord::Point(10, 40, 0)); // Left edge
        zone.contains(concord::Point(90, 40, 0)); // Right edge
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
        
        concord::Polygon l_boundary(l_points);
        Zone l_zone("L-Shape Zone", "field", l_boundary);
        
        // Points in different parts of the L
        CHECK(l_zone.contains(concord::Point(15, 15, 0))); // Bottom part
        CHECK(l_zone.contains(concord::Point(15, 45, 0))); // Left part
        CHECK(l_zone.contains(concord::Point(45, 15, 0))); // Right part
        
        // Point in the "notch" of the L
        CHECK(!l_zone.contains(concord::Point(45, 45, 0)));
        
        // Area should be: 60*30 + 30*30 = 1800 + 900 = 2700
        CHECK(l_zone.area() == doctest::Approx(2700.0));
    }
}

TEST_CASE("Zone ownership management") {
    Zone zone("Ownership Zone", "field");
    
    SUBCASE("Initial state") {
        CHECK(!zone.hasOwner());
        CHECK(zone.getOwnerRobot().isNull());
    }
    
    SUBCASE("Set and change owner") {
        auto robot1 = generateUUID();
        auto robot2 = generateUUID();
        
        // Set first owner
        zone.setOwnerRobot(robot1);
        CHECK(zone.hasOwner());
        CHECK(zone.getOwnerRobot() == robot1);
        CHECK(zone.getOwnerRobot() != robot2);
        
        // Change owner
        zone.setOwnerRobot(robot2);
        CHECK(zone.hasOwner());
        CHECK(zone.getOwnerRobot() == robot2);
        CHECK(zone.getOwnerRobot() != robot1);
        
        // Release ownership
        zone.releaseOwnership();
        CHECK(!zone.hasOwner());
        CHECK(zone.getOwnerRobot().isNull());
    }
}

TEST_CASE("Zone validation rules") {
    SUBCASE("Valid zones") {
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone valid_zone("Valid Zone", "field", boundary);
        CHECK(valid_zone.isValid());
        
        // Zone with just name and boundary is valid
        Zone minimal_zone("Minimal", "other", boundary);
        CHECK(minimal_zone.isValid());
    }
    
    SUBCASE("Invalid zones") {
        // No boundary
        Zone no_boundary("No Boundary", "field");
        CHECK(!no_boundary.isValid());
        
        // Empty name
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone empty_name("", "field", boundary);
        CHECK(!empty_name.isValid());
        
        // Both empty name and no boundary
        Zone completely_invalid("", "field");
        CHECK(!completely_invalid.isValid());
    }
}

TEST_CASE("Zone file I/O operations") {
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("File I/O Zone", "field", boundary);
    
    // Add some data to make it interesting
    concord::Grid<uint8_t> elevation_grid(5, 10, 10.0, true, concord::Pose{});
    for (size_t r = 0; r < 5; ++r) {
        for (size_t c = 0; c < 10; ++c) {
            elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
        }
    }
    zone.addElevationLayer(elevation_grid, "meters");
    
    // Add field elements
    std::vector<concord::Point> row_points;
    row_points.emplace_back(10, 25, 0);
    row_points.emplace_back(90, 25, 0);
    zone.addCropRow(concord::Path(row_points));
    
    SUBCASE("Save and load files") {
        const std::string vector_path = "/tmp/zoneout_test_zone.geojson";
        const std::string raster_path = "/tmp/zoneout_test_zone.tiff";
        
        // Save zone
        zone.toFiles(vector_path, raster_path);
        
        // Load zone back
        auto loaded_zone = Zone::fromFiles(vector_path, raster_path);
        
        // Basic properties should be preserved
        CHECK(loaded_zone.getName() == "File I/O Zone");
        CHECK(loaded_zone.getType() == "field");
        
        // Note: Detailed validation depends on actual geoson/geotiv implementation
        // This test mainly ensures the methods don't crash
    }
}

TEST_CASE("Zone property edge cases") {
    Zone zone("Edge Case Zone", "field");
    
    SUBCASE("Property overwrites") {
        zone.setProperty("test_key", "value1");
        CHECK(zone.getProperty("test_key") == "value1");
        
        zone.setProperty("test_key", "value2");
        CHECK(zone.getProperty("test_key") == "value2");
    }
    
    SUBCASE("Empty property values") {
        zone.setProperty("empty_key", "");
        CHECK(zone.getProperty("empty_key") == "");
        CHECK(zone.getProperty("empty_key", "default") == "");
    }
    
    SUBCASE("Special characters in properties") {
        zone.setProperty("special", "value with spaces and symbols!@#$%");
        CHECK(zone.getProperty("special") == "value with spaces and symbols!@#$%");
        
        zone.setProperty("unicode", "café naïve résumé");
        CHECK(zone.getProperty("unicode") == "café naïve résumé");
    }
    
    SUBCASE("Large number of properties") {
        // Add many properties
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i * 2);
            zone.setProperty(key, value);
        }
        
        // Verify they're all there
        auto properties = zone.getProperties();
        CHECK(properties.size() == 1000);
        
        // Check specific values
        CHECK(zone.getProperty("key_42") == "value_84");
        CHECK(zone.getProperty("key_999") == "value_1998");
    }
}