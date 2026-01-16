#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>

#include "geoson/vector.hpp"

namespace dp = datapod;

TEST_CASE("Vector Global Properties - Save and Load") {
    // Create a vector with boundary and elements
    dp::Polygon boundary;
    boundary.vertices = {{0.0, 0.0, 0.0}, {100.0, 0.0, 0.0}, {100.0, 100.0, 0.0}, {0.0, 100.0, 0.0}, {0.0, 0.0, 0.0}};

    dp::Geo datum{52.0, 4.0, 10.0};
    dp::Euler heading{0.0, 0.0, 1.5};

    geoson::Vector vector(boundary, datum, heading, geoson::CRS::ENU);

    // Set global properties
    vector.setGlobalProperty("uuid", "123e4567-e89b-12d3-a456-426614174000");
    vector.setGlobalProperty("name", "Test Field");
    vector.setGlobalProperty("type", "agricultural");
    vector.setGlobalProperty("subtype", "crop_field");
    vector.setGlobalProperty("owner", "Farm Co.");

    // Add some elements (polygon, line, points)
    dp::Polygon crop_area;
    crop_area.vertices = {
        {10.0, 10.0, 0.0}, {90.0, 10.0, 0.0}, {90.0, 90.0, 0.0}, {10.0, 90.0, 0.0}, {10.0, 10.0, 0.0}};
    std::unordered_map<std::string, std::string> crop_props;
    crop_props["crop_type"] = "corn";
    vector.addPolygon(crop_area, "crop_area", crop_props);

    // Add irrigation line
    dp::Segment irrigation_line({20.0, 5.0, 0.0}, {80.0, 5.0, 0.0});
    std::unordered_map<std::string, std::string> irrig_props;
    irrig_props["flow_rate"] = "50_gpm";
    vector.addLine(irrigation_line, "irrigation", irrig_props);

    // Add measurement points
    vector.addPoint({25.0, 25.0, 0.0}, "sensor", {{"sensor_id", "temp_01"}});
    vector.addPoint({75.0, 75.0, 0.0}, "sensor", {{"sensor_id", "moisture_02"}});

    const std::filesystem::path test_file = "/tmp/test_vector_global_props.geojson";

    SUBCASE("Save vector with global properties") {
        // Save to file
        vector.toFile(test_file, geoson::CRS::ENU);

        // Verify file exists
        CHECK(std::filesystem::exists(test_file));

        // Load back and verify global properties are preserved
        geoson::Vector loaded_vector = geoson::Vector::fromFile(test_file);

        // Check global properties
        CHECK(loaded_vector.getGlobalProperty("uuid") == "123e4567-e89b-12d3-a456-426614174000");
        CHECK(loaded_vector.getGlobalProperty("name") == "Test Field");
        CHECK(loaded_vector.getGlobalProperty("type") == "agricultural");
        CHECK(loaded_vector.getGlobalProperty("subtype") == "crop_field");
        CHECK(loaded_vector.getGlobalProperty("owner") == "Farm Co.");

        // Check that non-existent property returns default
        CHECK(loaded_vector.getGlobalProperty("non_existent") == "");
        CHECK(loaded_vector.getGlobalProperty("non_existent", "default") == "default");

        // Check all global properties
        auto global_props = loaded_vector.getGlobalProperties();
        CHECK(global_props.size() == 5);
        CHECK(global_props.at("uuid") == "123e4567-e89b-12d3-a456-426614174000");
        CHECK(global_props.at("name") == "Test Field");

        // Verify boundary and elements are preserved
        CHECK(!loaded_vector.getFieldBoundary().vertices.empty());
        CHECK(loaded_vector.elementCount() == 4); // 1 crop area + 1 irrigation + 2 sensors

        // Check specific elements
        auto crop_areas = loaded_vector.getElementsByType("crop_area");
        CHECK(crop_areas.size() == 1);
        CHECK(crop_areas[0].properties.at("crop_type") == "corn");

        auto sensors = loaded_vector.getElementsByType("sensor");
        CHECK(sensors.size() == 2);

        // Cleanup
        std::filesystem::remove(test_file);
    }

    SUBCASE("Modify global properties after loading") {
        // Save original
        vector.toFile(test_file, geoson::CRS::ENU);

        // Load and modify
        geoson::Vector loaded_vector = geoson::Vector::fromFile(test_file);
        loaded_vector.setGlobalProperty("modified", "true");
        loaded_vector.setGlobalProperty("name", "Modified Test Field");
        loaded_vector.removeGlobalProperty("owner");

        // Save modified version
        const std::filesystem::path modified_file = "/tmp/test_vector_modified.geojson";
        loaded_vector.toFile(modified_file, geoson::CRS::ENU);

        // Load modified version and verify changes
        geoson::Vector final_vector = geoson::Vector::fromFile(modified_file);

        CHECK(final_vector.getGlobalProperty("modified") == "true");
        CHECK(final_vector.getGlobalProperty("name") == "Modified Test Field");
        CHECK(final_vector.getGlobalProperty("owner") == "");                                    // Should be removed
        CHECK(final_vector.getGlobalProperty("uuid") == "123e4567-e89b-12d3-a456-426614174000"); // Should remain

        auto final_props = final_vector.getGlobalProperties();
        CHECK(final_props.size() == 5); // uuid, name, type, subtype, modified (owner removed)

        // Cleanup
        std::filesystem::remove(test_file);
        std::filesystem::remove(modified_file);
    }
}

TEST_CASE("Vector Global Properties - Empty Properties") {
    // Test vector with no global properties
    dp::Polygon boundary;
    boundary.vertices = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}};
    geoson::Vector vector(boundary, dp::Geo{0.001, 0.001, 1.0}, dp::Euler{0, 0, 0}, geoson::CRS::ENU);

    const std::filesystem::path test_file = "/tmp/test_vector_empty_props.geojson";

    // Save and load
    vector.toFile(test_file, geoson::CRS::ENU);
    geoson::Vector loaded_vector = geoson::Vector::fromFile(test_file);

    // Check empty global properties
    auto global_props = loaded_vector.getGlobalProperties();
    CHECK(global_props.empty());
    CHECK(loaded_vector.getGlobalProperty("any_key") == "");

    // Cleanup
    std::filesystem::remove(test_file);
}
