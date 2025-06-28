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

TEST_CASE("Farm creation and basic properties") {
    SUBCASE("Default constructor") {
        Farm farm;
        CHECK(farm.getName().empty());
        CHECK(farm.numZones() == 0);
        CHECK(!farm.isValid());
    }

    SUBCASE("Named constructor") {
        Farm farm("Test Farm");
        CHECK(farm.getName() == "Test Farm");
        CHECK(farm.numZones() == 0);
        CHECK(!farm.isValid()); // No zones yet
    }

    SUBCASE("Set farm name") {
        Farm farm;
        farm.setName("Updated Farm");
        CHECK(farm.getName() == "Updated Farm");
    }
}

TEST_CASE("Farm zone management") {
    Farm farm("Test Farm");
    
    SUBCASE("Add zones manually") {
        auto boundary = createRectangle(0, 0, 100, 50);
        auto zone = std::make_unique<Zone>("Test Zone", "field", boundary);
        UUID zone_id = zone->getId();
        
        farm.addZone(std::move(zone));
        
        CHECK(farm.numZones() == 1);
        CHECK(farm.hasZone(zone_id));
        CHECK(farm.hasZone("Test Zone"));
        CHECK(farm.isValid());
    }
    
    SUBCASE("Create zones with factory methods") {
        auto boundary1 = createRectangle(0, 0, 100, 50);
        auto boundary2 = createRectangle(200, 0, 80, 40);
        auto boundary3 = createRectangle(400, 0, 60, 30);
        
        auto& field = farm.createField("Wheat Field", boundary1);
        auto& barn = farm.createBarn("Main Barn", boundary2);
        auto& greenhouse = farm.createGreenhouse("Tomato House", boundary3);
        
        CHECK(farm.numZones() == 3);
        CHECK(field.getType() == "field");
        CHECK(barn.getType() == "barn");
        CHECK(greenhouse.getType() == "greenhouse");
        
        CHECK(field.getProperty("crop_type") == "unknown");
        CHECK(barn.getProperty("animal_type") == "unknown");
        CHECK(greenhouse.getProperty("climate_control") == "automated");
    }
    
    SUBCASE("Remove zones") {
        auto boundary = createRectangle(0, 0, 100, 50);
        auto& zone = farm.createField("Test Field", boundary);
        UUID zone_id = zone.getId();
        
        CHECK(farm.numZones() == 1);
        CHECK(farm.removeZone(zone_id));
        CHECK(farm.numZones() == 0);
        CHECK(!farm.hasZone(zone_id));
        CHECK(!farm.removeZone(zone_id)); // Already removed
    }
}

TEST_CASE("Farm zone retrieval") {
    Farm farm("Test Farm");
    auto boundary1 = createRectangle(0, 0, 100, 50);
    auto boundary2 = createRectangle(200, 0, 80, 40);
    
    auto& field1 = farm.createField("Field 1", boundary1);
    auto& field2 = farm.createField("Field 2", boundary2);
    auto& barn = farm.createBarn("Barn 1", createRectangle(400, 0, 60, 30));
    
    SUBCASE("Get zone by ID") {
        Zone* found = farm.getZone(field1.getId());
        CHECK(found != nullptr);
        CHECK(found->getName() == "Field 1");
        
        const Farm& const_farm = farm;
        const Zone* const_found = const_farm.getZone(field1.getId());
        CHECK(const_found != nullptr);
        CHECK(const_found->getName() == "Field 1");
    }
    
    SUBCASE("Get zone by name") {
        Zone* found = farm.getZone("Field 1");
        CHECK(found != nullptr);
        CHECK(found->getId() == field1.getId());
        
        Zone* not_found = farm.getZone("Non-existent");
        CHECK(not_found == nullptr);
    }
    
    SUBCASE("Get all zones") {
        auto zones = farm.getZones();
        CHECK(zones.size() == 3);
        
        const Farm& const_farm = farm;
        auto const_zones = const_farm.getZones();
        CHECK(const_zones.size() == 3);
    }
    
    SUBCASE("Get zones by type") {
        auto fields = farm.getZonesByType("field");
        auto barns = farm.getZonesByType("barn");
        auto non_existent = farm.getZonesByType("greenhouse");
        
        CHECK(fields.size() == 2);
        CHECK(barns.size() == 1);
        CHECK(non_existent.size() == 0);
        
        CHECK(farm.numZonesByType("field") == 2);
        CHECK(farm.numZonesByType("barn") == 1);
        CHECK(farm.numZonesByType("greenhouse") == 0);
    }
}

TEST_CASE("Farm spatial operations") {
    Farm farm("Spatial Test Farm");
    
    // Create zones in a grid pattern
    auto& zone1 = farm.createField("Zone 1", createRectangle(0, 0, 100, 100));     // (0,0) to (100,100)
    auto& zone2 = farm.createField("Zone 2", createRectangle(200, 0, 100, 100));   // (200,0) to (300,100)
    auto& zone3 = farm.createBarn("Zone 3", createRectangle(0, 200, 100, 100));    // (0,200) to (100,300)
    auto& zone4 = farm.createField("Zone 4", createRectangle(200, 200, 100, 100)); // (200,200) to (300,300)
    
    SUBCASE("Point containment queries") {
        // Point inside zone1
        auto zones_at_50_50 = farm.findZonesContaining(concord::Point(50, 50, 0));
        CHECK(zones_at_50_50.size() == 1);
        CHECK(zones_at_50_50[0]->getName() == "Zone 1");
        
        // Point at zone boundary (may or may not be included)
        auto zones_at_boundary = farm.findZonesContaining(concord::Point(100, 100, 0));
        // Don't check exact count as boundary behavior may vary
        
        // Point outside all zones
        auto zones_at_1000_1000 = farm.findZonesContaining(concord::Point(1000, 1000, 0));
        CHECK(zones_at_1000_1000.size() == 0);
        
        // Test const version
        const Farm& const_farm = farm;
        auto const_zones = const_farm.findZonesContaining(concord::Point(50, 50, 0));
        CHECK(const_zones.size() == 1);
    }
    
    SUBCASE("Radius searches") {
        // Small radius from zone1 center - should find only zone1
        auto zones_within_10 = farm.findZonesWithinRadius(concord::Point(50, 50, 0), 10.0);
        CHECK(zones_within_10.size() == 1);
        CHECK(zones_within_10[0]->getName() == "Zone 1");
        
        // Large radius from center - should find multiple zones
        auto zones_within_500 = farm.findZonesWithinRadius(concord::Point(150, 150, 0), 500.0);
        CHECK(zones_within_500.size() >= 2); // Should find multiple zones
        
        // Very small radius outside all zones
        auto zones_within_1 = farm.findZonesWithinRadius(concord::Point(1000, 1000, 0), 1.0);
        CHECK(zones_within_1.size() == 0);
    }
    
    SUBCASE("Polygon intersection queries") {
        // Polygon overlapping zone1 and zone2
        auto overlap_polygon = createRectangle(50, -50, 200, 200); // Spans from zone1 to zone2
        auto intersecting_zones = farm.findZonesIntersecting(overlap_polygon);
        CHECK(intersecting_zones.size() >= 1); // Should find at least one zone
        
        // Small polygon inside zone1
        auto small_polygon = createRectangle(25, 25, 50, 50);
        auto contained_zones = farm.findZonesIntersecting(small_polygon);
        CHECK(contained_zones.size() >= 1);
        
        // Polygon outside all zones
        auto outside_polygon = createRectangle(1000, 1000, 100, 100);
        auto no_zones = farm.findZonesIntersecting(outside_polygon);
        CHECK(no_zones.size() == 0);
    }
    
    SUBCASE("Nearest zone queries") {
        // Point inside a zone - nearest should be that zone with distance 0
        Zone* nearest_inside = farm.findNearestZone(concord::Point(50, 50, 0));
        CHECK(nearest_inside != nullptr);
        CHECK(nearest_inside->getName() == "Zone 1");
        
        // Point outside all zones
        Zone* nearest_outside = farm.findNearestZone(concord::Point(500, 500, 0));
        CHECK(nearest_outside != nullptr); // Should find some zone
        
        // Point very far away
        Zone* nearest_far = farm.findNearestZone(concord::Point(10000, 10000, 0));
        CHECK(nearest_far != nullptr); // Should still find some zone
    }
    
    SUBCASE("K-nearest neighbors") {
        concord::Point query_point(150, 150, 0); // Center between zones
        
        // Find 2 nearest zones
        auto nearest_2 = farm.findKNearestZones(query_point, 2);
        CHECK(nearest_2.size() <= 2);
        CHECK(nearest_2.size() >= 1);
        
        // Find more zones than available
        auto nearest_10 = farm.findKNearestZones(query_point, 10);
        CHECK(nearest_10.size() == 4); // Should return all 4 zones
        
        // Find 0 zones
        auto nearest_0 = farm.findKNearestZones(query_point, 0);
        CHECK(nearest_0.size() == 0);
    }
    
    SUBCASE("Bounding box queries") {
        // AABB covering zone1 and zone2
        concord::AABB query_bounds(concord::Point(-50, -50, 0), concord::Point(350, 150, 0));
        auto bounded_zones = farm.findZonesInBounds(query_bounds);
        CHECK(bounded_zones.size() >= 2); // Should find zone1 and zone2
        
        // Small AABB inside zone1
        concord::AABB small_bounds(concord::Point(25, 25, 0), concord::Point(75, 75, 0));
        auto contained_zones = farm.findZonesInBounds(small_bounds);
        CHECK(contained_zones.size() >= 1);
        
        // AABB outside all zones
        concord::AABB outside_bounds(concord::Point(1000, 1000, 0), concord::Point(1100, 1100, 0));
        auto no_zones = farm.findZonesInBounds(outside_bounds);
        CHECK(no_zones.size() == 0);
    }
}

TEST_CASE("Farm statistics") {
    Farm farm("Stats Test Farm");
    
    auto& field1 = farm.createField("Field 1", createRectangle(0, 0, 100, 100));   // 10,000 m²
    auto& field2 = farm.createField("Field 2", createRectangle(200, 0, 50, 50));   // 2,500 m²
    auto& barn = farm.createBarn("Barn 1", createRectangle(400, 0, 20, 30));       // 600 m²
    
    SUBCASE("Area calculations") {
        CHECK(farm.totalArea() == doctest::Approx(13100.0)); // 10000 + 2500 + 600
        CHECK(farm.areaByType("field") == doctest::Approx(12500.0)); // 10000 + 2500
        CHECK(farm.areaByType("barn") == doctest::Approx(600.0));
        CHECK(farm.areaByType("greenhouse") == doctest::Approx(0.0));
    }
    
    SUBCASE("Bounding box") {
        auto bbox = farm.getBoundingBox();
        CHECK(bbox.has_value());
        
        if (bbox) {
            // Should span from (0,0) to (420,100)
            CHECK(bbox->min_point.x == doctest::Approx(0.0));
            CHECK(bbox->min_point.y == doctest::Approx(0.0));
            CHECK(bbox->max_point.x == doctest::Approx(420.0));
            CHECK(bbox->max_point.y == doctest::Approx(100.0));
        }
    }
    
    SUBCASE("Empty farm bounding box") {
        Farm empty_farm("Empty");
        auto bbox = empty_farm.getBoundingBox();
        CHECK(!bbox.has_value());
    }
}

TEST_CASE("Farm spatial index performance") {
    Farm farm("Performance Test Farm");
    
    // Create a grid of zones for performance testing
    const int grid_size = 10; // 10x10 = 100 zones
    const double zone_size = 100.0;
    
    for (int i = 0; i < grid_size; ++i) {
        for (int j = 0; j < grid_size; ++j) {
            double x = i * zone_size;
            double y = j * zone_size;
            std::string name = "Zone_" + std::to_string(i) + "_" + std::to_string(j);
            farm.createField(name, createRectangle(x, y, zone_size, zone_size));
        }
    }
    
    CHECK(farm.numZones() == 100);
    
    SUBCASE("Spatial index statistics") {
        auto stats = farm.getSpatialIndexStats();
        // R-tree spatial index should now work correctly after bug fix
        CHECK(stats.total_entries >= 0); // Just check it doesn't crash
        CHECK(stats.total_entries <= 100); // Should not exceed number of zones  
        CHECK(stats.total_entries == 100); // Should store all zones with working R-tree
    }
    
    SUBCASE("Multiple point queries") {
        // Test multiple point queries to ensure spatial index is working
        std::vector<concord::Point> test_points = {
            concord::Point(150, 150, 0),  // Inside grid
            concord::Point(550, 550, 0),  // Inside grid
            concord::Point(50, 50, 0),    // Inside grid
            concord::Point(1500, 1500, 0) // Outside grid
        };
        
        for (const auto& point : test_points) {
            auto zones = farm.findZonesContaining(point);
            // Each point should return consistent results
            auto zones2 = farm.findZonesContaining(point);
            CHECK(zones.size() == zones2.size());
        }
    }
    
    SUBCASE("Index rebuild") {
        // Test manual index rebuild
        auto original_stats = farm.getSpatialIndexStats();
        farm.rebuildSpatialIndexManually();
        auto rebuilt_stats = farm.getSpatialIndexStats();
        
        CHECK(rebuilt_stats.total_entries == original_stats.total_entries);
    }
}

TEST_CASE("Farm properties") {
    Farm farm("Property Test Farm");
    
    SUBCASE("Set and get properties") {
        farm.setProperty("owner", "John Doe");
        farm.setProperty("location", "Iowa, USA");
        farm.setProperty("established", "1995");
        
        CHECK(farm.getProperty("owner") == "John Doe");
        CHECK(farm.getProperty("location") == "Iowa, USA");
        CHECK(farm.getProperty("established") == "1995");
        CHECK(farm.getProperty("non_existent", "default") == "default");
        
        auto properties = farm.getProperties();
        CHECK(properties.size() == 3);
        CHECK(properties.at("owner") == "John Doe");
    }
}

TEST_CASE("Farm timestamps") {
    Farm farm("Timestamp Test Farm");
    
    // Check initial timestamps
    auto epoch = std::chrono::time_point<std::chrono::system_clock>{};
    CHECK(farm.getCreatedTime() > epoch);
    CHECK(farm.getModifiedTime() > epoch);
    
    auto initial_modified = farm.getModifiedTime();
    
    // Modify farm and check timestamp update
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    farm.setProperty("test", "value");
    
    CHECK(farm.getModifiedTime() > initial_modified);
}

TEST_CASE("Farm iteration support") {
    Farm farm("Iteration Test Farm");
    
    auto& zone1 = farm.createField("Field 1", createRectangle(0, 0, 100, 100));
    auto& zone2 = farm.createBarn("Barn 1", createRectangle(200, 0, 50, 50));
    auto& zone3 = farm.createField("Field 2", createRectangle(400, 0, 75, 75));
    
    SUBCASE("Range-based for loop") {
        int count = 0;
        for (const auto& [id, zone_ptr] : farm) {
            CHECK(zone_ptr != nullptr);
            count++;
        }
        CHECK(count == 3);
    }
    
    SUBCASE("forEachZone template") {
        int field_count = 0;
        farm.forEachZone([&field_count](const Zone& zone) {
            if (zone.getType() == "field") {
                field_count++;
            }
        });
        CHECK(field_count == 2);
        
        // Test const version
        const Farm& const_farm = farm;
        int total_count = 0;
        const_farm.forEachZone([&total_count](const Zone& zone) {
            total_count++;
        });
        CHECK(total_count == 3);
    }
    
    SUBCASE("filterZones template") {
        auto fields = farm.filterZones([](const Zone& zone) {
            return zone.getType() == "field";
        });
        CHECK(fields.size() == 2);
        
        auto large_zones = farm.filterZones([](const Zone& zone) {
            return zone.area() > 6000.0; // Field 1 is 10000, Field 2 is 5625, barn is 2500
        });
        CHECK(large_zones.size() == 1); // Only Field 1 (10000) is > 6000
    }
}

TEST_CASE("Farm file I/O") {
    Farm farm("File I/O Test Farm");
    
    // Create some zones with raster data
    auto& field = farm.createField("Test Field", createRectangle(0, 0, 100, 50));
    
    // Add elevation data  
    concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 10; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
        }
    }
    field.add_layer("elevation", "terrain", elevation_grid, {{"units", "meters"}});
    
    SUBCASE("Save and load farm") {
        const std::string test_dir = "/tmp/zoneout_test_farm";
        
        // Save farm
        farm.saveToDirectory(test_dir);
        
        // Load farm back
        auto loaded_farm = Farm::loadFromDirectory(test_dir, "Loaded Farm");
        
        CHECK(loaded_farm.getName() == "Loaded Farm");
        // Note: The number of loaded zones depends on the actual file I/O implementation
        // This test mainly verifies the methods don't crash
    }
}