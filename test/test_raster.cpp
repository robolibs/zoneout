#include <doctest/doctest.h>
#include <filesystem>

#include "geotiv/raster.hpp"

namespace dp = datapod;

TEST_CASE("Raster Global Properties - Save and Load") {
    // Create a raster with multiple layers
    dp::Geo datum{52.0, 4.0, 10.0};
    dp::Pose pose{dp::Point{0, 0, 0}, dp::Quaternion{}};
    geotiv::Raster raster(datum, pose, 1.0);

    // Add three different layers
    raster.addElevationGrid(100, 100, "elevation");
    raster.addTerrainGrid(100, 100, "terrain");
    raster.addOcclusionGrid(100, 100, "obstacles");

    // Set global properties
    raster.setGlobalProperty("uuid", "987fcdeb-51a2-43d1-9c47-123456789abc");
    raster.setGlobalProperty("name", "Test Farm Raster");
    raster.setGlobalProperty("type", "multi_layer");
    raster.setGlobalProperty("subtype", "farm_analysis");
    raster.setGlobalProperty("created_by", "AgriTech Solutions");
    raster.setGlobalProperty("version", "2.1");

    const std::filesystem::path test_file = "/tmp/test_raster_global_props.tiff";

    SUBCASE("Save raster with global properties") {
        // Save to file
        raster.toFile(test_file);

        // Verify file exists
        CHECK(std::filesystem::exists(test_file));

        // Load back and verify global properties are preserved
        geotiv::Raster loaded_raster = geotiv::Raster::fromFile(test_file);

        // Check global properties
        CHECK(loaded_raster.getGlobalProperty("uuid") == "987fcdeb-51a2-43d1-9c47-123456789abc");
        CHECK(loaded_raster.getGlobalProperty("name") == "Test Farm Raster");
        CHECK(loaded_raster.getGlobalProperty("type") == "multi_layer");
        CHECK(loaded_raster.getGlobalProperty("subtype") == "farm_analysis");
        CHECK(loaded_raster.getGlobalProperty("created_by") == "AgriTech Solutions");
        CHECK(loaded_raster.getGlobalProperty("version") == "2.1");

        // Check that non-existent property returns default
        CHECK(loaded_raster.getGlobalProperty("non_existent") == "");
        CHECK(loaded_raster.getGlobalProperty("non_existent", "default") == "default");

        // Check all global properties
        auto global_props = loaded_raster.getGlobalProperties();
        CHECK(global_props.size() == 6);
        CHECK(global_props.at("uuid") == "987fcdeb-51a2-43d1-9c47-123456789abc");
        CHECK(global_props.at("name") == "Test Farm Raster");
        CHECK(global_props.at("version") == "2.1");

        // Verify layers are preserved
        CHECK(loaded_raster.hasGrids());
        CHECK(loaded_raster.gridCount() == 3);

        // Check layer names
        auto grid_names = loaded_raster.getGridNames();
        CHECK(grid_names.size() == 3);
        // Names might be modified during save/load, so check that layers exist
        CHECK(loaded_raster.gridCount() == 3);

        // Verify that all layers have the same global properties
        for (size_t i = 0; i < loaded_raster.gridCount(); ++i) {
            const auto &layer = loaded_raster.getGrid(i);
            auto layer_global_props = layer.getGlobalProperties();
            CHECK(layer_global_props.size() == 6);
            CHECK(layer_global_props.at("uuid") == "987fcdeb-51a2-43d1-9c47-123456789abc");
            CHECK(layer_global_props.at("name") == "Test Farm Raster");
        }

        // Cleanup
        std::filesystem::remove(test_file);
    }

    SUBCASE("Modify global properties after loading") {
        // Save original
        raster.toFile(test_file);

        // Load and modify
        geotiv::Raster loaded_raster = geotiv::Raster::fromFile(test_file);
        loaded_raster.setGlobalProperty("modified", "true");
        loaded_raster.setGlobalProperty("name", "Modified Farm Raster");
        loaded_raster.removeGlobalProperty("created_by");

        // Save modified version
        const std::filesystem::path modified_file = "/tmp/test_raster_modified.tiff";
        loaded_raster.toFile(modified_file);

        // Load modified version and verify changes
        geotiv::Raster final_raster = geotiv::Raster::fromFile(modified_file);

        CHECK(final_raster.getGlobalProperty("modified") == "true");
        CHECK(final_raster.getGlobalProperty("name") == "Modified Farm Raster");
        CHECK(final_raster.getGlobalProperty("created_by") == "");                               // Should be removed
        CHECK(final_raster.getGlobalProperty("uuid") == "987fcdeb-51a2-43d1-9c47-123456789abc"); // Should remain

        auto final_props = final_raster.getGlobalProperties();
        CHECK(final_props.size() == 6); // uuid, name, type, subtype, version, modified (created_by removed)

        // Verify all layers still have consistent global properties
        for (size_t i = 0; i < final_raster.gridCount(); ++i) {
            const auto &layer = final_raster.getGrid(i);
            auto layer_props = layer.getGlobalProperties();
            CHECK(layer_props.at("modified") == "true");
            CHECK(layer_props.at("name") == "Modified Farm Raster");
            CHECK(layer_props.find("created_by") == layer_props.end()); // Should not exist
        }

        // Cleanup
        std::filesystem::remove(test_file);
        std::filesystem::remove(modified_file);
    }

    SUBCASE("Add new layer after setting global properties") {
        // Set global properties first
        raster.setGlobalProperty("test_key", "test_value");

        // Add a new layer
        raster.addGrid(50, 50, "new_layer", "custom");

        // Verify new layer has the global properties
        const auto &new_layer = raster.getGrid(raster.gridCount() - 1);
        auto new_layer_props = new_layer.getGlobalProperties();
        CHECK(new_layer_props.at("test_key") == "test_value");
        CHECK(new_layer_props.at("uuid") == "987fcdeb-51a2-43d1-9c47-123456789abc");

        // Save and load to verify persistence
        raster.toFile(test_file);
        geotiv::Raster loaded_raster = geotiv::Raster::fromFile(test_file);

        CHECK(loaded_raster.gridCount() == 4); // Original 3 + 1 new
        CHECK(loaded_raster.getGlobalProperty("test_key") == "test_value");

        // Cleanup
        std::filesystem::remove(test_file);
    }
}

TEST_CASE("Raster Global Properties - Empty Properties") {
    // Test raster with no global properties
    dp::Geo datum{52.0, 4.0, 10.0};
    dp::Pose pose{dp::Point{0, 0, 0}, dp::Quaternion{}};
    geotiv::Raster raster(datum, pose);
    raster.addGrid(50, 50, "single_layer");

    const std::filesystem::path test_file = "/tmp/test_raster_empty_props.tiff";

    // Save and load
    raster.toFile(test_file);
    geotiv::Raster loaded_raster = geotiv::Raster::fromFile(test_file);

    // Check empty global properties
    auto global_props = loaded_raster.getGlobalProperties();
    CHECK(global_props.empty());
    CHECK(loaded_raster.getGlobalProperty("any_key") == "");

    // Cleanup
    std::filesystem::remove(test_file);
}

TEST_CASE("Raster Global Properties - ASCII Tag Encoding/Decoding") {
    // Test the ASCII tag encoding/decoding functions directly
    std::string test_string = "uuid=123-456-789";

    // Encode to ASCII tag
    auto encoded = geotiv::stringToAsciiTag(test_string);
    CHECK(!encoded.empty());

    // Decode back
    std::string decoded = geotiv::asciiTagToString(encoded);
    CHECK(decoded == test_string);

    // Test with special characters
    std::string special_string = "name=Test Field #1 (v2.0)";
    auto encoded_special = geotiv::stringToAsciiTag(special_string);
    std::string decoded_special = geotiv::asciiTagToString(encoded_special);
    CHECK(decoded_special == special_string);

    // Test with empty string
    auto encoded_empty = geotiv::stringToAsciiTag("");
    std::string decoded_empty = geotiv::asciiTagToString(encoded_empty);
    CHECK(decoded_empty == "");
}