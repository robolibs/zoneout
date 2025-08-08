#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "zoneout/zoneout.hpp"
#include <filesystem>

using namespace zoneout;

TEST_CASE("Zone directory save/load functionality") {
    concord::Datum datum{51.98776, 5.66238, 0.0};
    
    // Create a simple rectangular boundary
    concord::Polygon boundary;
    boundary.addPoint({0, 0, 0});
    boundary.addPoint({100, 0, 0});
    boundary.addPoint({100, 50, 0});
    boundary.addPoint({0, 50, 0});
    
    SUBCASE("Zone save() and load() methods") {
        // Create zone with auto-generated grid
        Zone zone("Test Zone", "field", boundary, datum);
        zone.setProperty("crop_type", "wheat");
        zone.setProperty("test_prop", "test_value");
        
        std::string test_dir = "/tmp/test_zone_save_dir";
        std::filesystem::remove_all(test_dir);
        
        // Test save method
        REQUIRE_NOTHROW(zone.save(test_dir));
        CHECK(std::filesystem::exists(test_dir + "/vector.geojson"));
        CHECK(std::filesystem::exists(test_dir + "/raster.tiff"));
        
        // Test load method
        Zone loaded_zone = Zone::load(test_dir);
        
        // Verify data integrity
        CHECK(zone.getName() == loaded_zone.getName());
        CHECK(zone.getId().toString() == loaded_zone.getId().toString());
        CHECK(zone.getProperty("crop_type") == loaded_zone.getProperty("crop_type"));
        CHECK(zone.getProperty("test_prop") == loaded_zone.getProperty("test_prop"));
        
        // Cleanup
        std::filesystem::remove_all(test_dir);
    }
}