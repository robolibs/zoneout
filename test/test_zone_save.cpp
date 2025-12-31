#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "zoneout/zoneout.hpp"
#include <filesystem>

namespace dp = datapod;
using namespace zoneout;

TEST_CASE("Zone directory save/load functionality") {
    dp::Geo datum{51.98776, 5.66238, 0.0};

    // Create a simple rectangular boundary
    dp::Polygon boundary;
    boundary.vertices.push_back({0, 0, 0});
    boundary.vertices.push_back({100, 0, 0});
    boundary.vertices.push_back({100, 50, 0});
    boundary.vertices.push_back({0, 50, 0});

    SUBCASE("Zone save() and load() methods") {
        // Create zone with auto-generated grid
        Zone zone("Test Zone", "field", boundary, datum);
        zone.set_property("crop_type", "wheat");
        zone.set_property("test_prop", "test_value");

        std::string test_dir = "/tmp/test_zone_save_dir";
        std::filesystem::remove_all(test_dir);

        // Test save method
        REQUIRE_NOTHROW(zone.save(test_dir));
        CHECK(std::filesystem::exists(test_dir + "/vector.geojson"));
        CHECK(std::filesystem::exists(test_dir + "/raster.tiff"));

        // Test load method
        Zone loaded_zone = Zone::load(test_dir);

        // Verify data integrity
        CHECK(zone.name() == loaded_zone.name());
        CHECK(zone.id().toString() == loaded_zone.id().toString());
        CHECK(zone.get_property("crop_type") == loaded_zone.get_property("crop_type"));
        CHECK(zone.get_property("test_prop") == loaded_zone.get_property("test_prop"));

        // Cleanup
        std::filesystem::remove_all(test_dir);
    }
}