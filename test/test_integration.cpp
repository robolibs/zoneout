#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

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

TEST_CASE("Complete agricultural scenario") {
    SUBCASE("Multi-robot farm coordination scenario") {
        Farm smart_farm("Smart Agricultural Test Farm");
        
        // Create robot IDs
        auto harvester_id = generateUUID();
        auto sprayer_id = generateUUID();
        auto surveyor_id = generateUUID();
        
        // Create farm layout
        auto& north_field = smart_farm.createField("North Field", createRectangle(0, 0, 500, 300));
        auto& south_field = smart_farm.createField("South Field", createRectangle(0, -400, 500, 300));
        auto& main_barn = smart_farm.createBarn("Main Barn", createRectangle(600, -50, 100, 150));
        auto& equipment_shed = smart_farm.createBarn("Equipment Shed", createRectangle(600, 150, 80, 100));
        auto& greenhouse = smart_farm.createGreenhouse("Tomato Greenhouse", createRectangle(750, 0, 150, 100));
        
        // Set up field properties
        north_field.setProperty("crop_type", "wheat");
        north_field.setProperty("planting_date", "2024-03-15");
        north_field.setProperty("growth_stage", "heading");
        north_field.setProperty("irrigation_schedule", "daily_morning");
        
        south_field.setProperty("crop_type", "corn");
        south_field.setProperty("planting_date", "2024-04-01");
        south_field.setProperty("growth_stage", "vegetative");
        south_field.setProperty("irrigation_schedule", "every_other_day");
        
        // Add field elements to north field
        for (int row = 0; row < 15; ++row) {
            std::vector<concord::Point> row_points;
            double y = 20.0 + row * 18.0;
            row_points.emplace_back(10, y, 0);
            row_points.emplace_back(490, y, 0);
            
            std::unordered_map<std::string, std::string> row_props;
            row_props["row_number"] = std::to_string(row + 1);
            row_props["crop_variety"] = "winter_wheat_premium";
            row_props["seeding_rate"] = "120kg_per_hectare";
            
            north_field.add_element(concord::Path(row_points), "crop_row", row_props);
        }
        
        // Add irrigation system
        std::vector<concord::Point> main_irrigation;
        main_irrigation.emplace_back(50, 150, 0);
        main_irrigation.emplace_back(450, 150, 0);
        
        std::unordered_map<std::string, std::string> irrigation_props;
        irrigation_props["flow_rate"] = "200L_per_minute";
        irrigation_props["pressure"] = "4.5_bar";
        irrigation_props["coverage_width"] = "30_meters";
        
        north_field.add_element(concord::Path(main_irrigation), "irrigation_line", irrigation_props);
        
        // Add elevation data to north field
        // Grid should cover the field area (0,0 to 500,300), so center it at (250,150)
        concord::Pose grid_pose;
        grid_pose.point = concord::Point(250, 150, 0); // Center of the field
        concord::Grid<uint8_t> elevation_grid(25, 50, 10.0, true, grid_pose);
        for (size_t r = 0; r < 25; ++r) {
            for (size_t c = 0; c < 50; ++c) {
                // Simulate gentle slope from 95m to 105m elevation
                uint8_t elevation = static_cast<uint8_t>(95 + (r + c) * 10 / (25 + 50));
                elevation_grid.set_value(r, c, elevation);
            }
        }
        north_field.add_layer("elevation", "terrain", elevation_grid, {{"units", "meters"}});
        
        // Add soil moisture data
        concord::Grid<uint8_t> moisture_grid(25, 50, 10.0, true, grid_pose);
        for (size_t r = 0; r < 25; ++r) {
            for (size_t c = 0; c < 50; ++c) {
                // Simulate moisture variation (20-60%)
                uint8_t moisture = static_cast<uint8_t>(20 + (r * c + r + c) % 40);
                moisture_grid.set_value(r, c, moisture);
            }
        }
        north_field.add_layer("soil_moisture", "environmental", moisture_grid, {{"units", "percentage"}});
        
        // Robot task assignment
        north_field.setOwnerRobot(harvester_id);
        south_field.setOwnerRobot(sprayer_id);
        
        // Verify farm setup
        CHECK(smart_farm.numZones() == 5);
        CHECK(smart_farm.numZonesByType("field") == 2);
        CHECK(smart_farm.numZonesByType("barn") == 2);
        CHECK(smart_farm.numZonesByType("greenhouse") == 1);
        
        CHECK(smart_farm.totalArea() == doctest::Approx(150000.0 + 150000.0 + 15000.0 + 8000.0 + 15000.0));
        
        // Test spatial queries for robot navigation
        concord::Point robot_position(250, 150, 0); // Robot in north field
        auto current_zones = smart_farm.findZonesContaining(robot_position);
        CHECK(current_zones.size() >= 1);
        CHECK(current_zones[0]->getName() == "North Field");
        
        // Find nearest barn for refueling
        auto nearest_barn = smart_farm.findNearestZone(robot_position);
        CHECK(nearest_barn != nullptr);
        
        // Find all zones within operational radius
        auto operational_zones = smart_farm.findZonesWithinRadius(robot_position, 200.0);
        CHECK(operational_zones.size() >= 1);
        
        // Sample environmental data at robot position
        if (current_zones[0]->has_layer("elevation")) {
            auto elevation = current_zones[0]->sample_at("elevation", robot_position);
            CHECK(elevation.has_value());
            CHECK(*elevation >= 95);
            CHECK(*elevation <= 105);
        }
        
        if (current_zones[0]->has_layer("soil_moisture")) {
            auto moisture = current_zones[0]->sample_at("soil_moisture", robot_position);
            CHECK(moisture.has_value());
            CHECK(*moisture >= 20);
            CHECK(*moisture <= 60);
        }
        
        // Test zone ownership
        CHECK(north_field.hasOwner());
        CHECK(north_field.getOwnerRobot() == harvester_id);
        CHECK(south_field.getOwnerRobot() == sprayer_id);
        CHECK(!main_barn.hasOwner());
        
        // Simulate robot task completion and ownership release
        north_field.releaseOwnership();
        CHECK(!north_field.hasOwner());
        
        // Reassign to surveyor robot
        north_field.setOwnerRobot(surveyor_id);
        CHECK(north_field.getOwnerRobot() == surveyor_id);
        
        // Verify field elements
        auto crop_rows = north_field.get_elements("crop_row");
        CHECK(crop_rows.size() == 15);
        
        auto irrigation_lines = north_field.get_elements("irrigation_line");
        CHECK(irrigation_lines.size() == 1);
        
        auto all_elements = north_field.get_elements();
        CHECK(all_elements.size() == 16); // 15 crop rows + 1 irrigation line
        
        // Test raster layers
        CHECK(north_field.num_layers() == 2);
        CHECK(north_field.has_layer("elevation"));
        CHECK(north_field.has_layer("soil_moisture"));
        
        auto layer_names = north_field.get_layer_names();
        CHECK(layer_names.size() == 2);
    }
}

TEST_CASE("Time-based operations and synchronization") {
    SUBCASE("Zone modification timestamps") {
        Farm time_farm("Time Test Farm");
        
        auto initial_farm_time = time_farm.getCreatedTime();
        auto initial_modified = time_farm.getModifiedTime();
        
        // Create zone and check timestamps
        auto& zone = time_farm.createField("Time Zone", createRectangle(0, 0, 100, 100));
        auto zone_created = zone.getCreatedTime();
        auto zone_modified = zone.getModifiedTime();
        
        CHECK(zone_created >= initial_farm_time);
        CHECK(zone_modified >= zone_created);
        CHECK(time_farm.getModifiedTime() > initial_modified);
        
        // Wait and modify zone
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto before_property_change = zone.getModifiedTime();
        
        zone.setProperty("test_modification", "test_value");
        
        CHECK(zone.getModifiedTime() > before_property_change);
        
        // Test Lamport clock functionality
        LamportClock robot_clock;
        auto t1 = robot_clock.tick();
        auto t2 = robot_clock.tick();
        CHECK(t2 > t1);
        
        // Simulate receiving timestamp from another robot
        auto t3 = robot_clock.update(t2 + 5);
        CHECK(t3 > t2 + 5);
        CHECK(robot_clock.time() == t3);
    }
}

TEST_CASE("Memory and performance stress tests") {
    SUBCASE("Large number of zones") {
        Farm stress_farm("Stress Test Farm");
        
        const int num_zones = 1000;
        const double zone_size = 50.0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create many zones
        for (int i = 0; i < num_zones; ++i) {
            double x = (i % 50) * zone_size;
            double y = (i / 50) * zone_size;
            std::string name = "StressZone_" + std::to_string(i);
            
            auto& zone = stress_farm.createField(name, createRectangle(x, y, zone_size, zone_size));
            
            // Add some properties
            zone.setProperty("id", std::to_string(i));
            zone.setProperty("category", i % 5 == 0 ? "premium" : "standard");
            
            // Add raster data to every 10th zone
            if (i % 10 == 0) {
                concord::Grid<uint8_t> test_grid(5, 5, 10.0, true, concord::Pose{});
                for (size_t r = 0; r < 5; ++r) {
                    for (size_t c = 0; c < 5; ++c) {
                        test_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c + i));
                    }
                }
                zone.add_layer("elevation", "terrain", test_grid, {{"units", "meters"}});
            }
        }
        
        auto creation_end = std::chrono::high_resolution_clock::now();
        auto creation_time = std::chrono::duration_cast<std::chrono::milliseconds>(creation_end - start_time);
        
        CHECK(stress_farm.numZones() == num_zones);
        
        // Test query performance
        auto query_start = std::chrono::high_resolution_clock::now();
        
        int total_queries = 100;
        int total_found = 0;
        
        for (int i = 0; i < total_queries; ++i) {
            double x = (i * 17) % 2500; // Pseudo-random positions
            double y = (i * 23) % 1000;
            concord::Point query_point(x, y, 0);
            
            auto zones = stress_farm.findZonesContaining(query_point);
            total_found += zones.size();
        }
        
        auto query_end = std::chrono::high_resolution_clock::now();
        auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);
        
        // Performance bounds (these are generous to account for different hardware)
        CHECK(creation_time.count() < 30000); // Less than 30 seconds
        CHECK(query_time.count() < 50000);    // Less than 50ms for 100 queries
        CHECK(total_found > 0); // Should find some zones
        
        // Test spatial index statistics
        auto stats = stress_farm.getSpatialIndexStats();
        CHECK(stats.total_entries == num_zones);
        
        // Test memory cleanup by removing zones
        auto removal_start = std::chrono::high_resolution_clock::now();
        
        int removed_count = 0;
        for (int i = 0; i < num_zones; i += 10) {
            std::string name = "StressZone_" + std::to_string(i);
            auto* zone = stress_farm.getZone(name);
            if (zone) {
                bool removed = stress_farm.removeZone(zone->getId());
                if (removed) removed_count++;
            }
        }
        
        auto removal_end = std::chrono::high_resolution_clock::now();
        auto removal_time = std::chrono::duration_cast<std::chrono::milliseconds>(removal_end - removal_start);
        
        CHECK(removed_count == 100); // Should remove every 10th zone
        CHECK(stress_farm.numZones() == num_zones - removed_count);
        CHECK(removal_time.count() < 5000); // Less than 5 seconds
    }
}

TEST_CASE("Error handling and edge cases") {
    SUBCASE("Invalid zone operations") {
        Farm error_farm("Error Test Farm");
        
        // Try to get non-existent zone
        auto* non_existent = error_farm.getZone("NonExistent");
        CHECK(non_existent == nullptr);
        
        auto fake_id = generateUUID();
        auto* non_existent_by_id = error_farm.getZone(fake_id);
        CHECK(non_existent_by_id == nullptr);
        
        // Try to remove non-existent zone
        bool removed = error_farm.removeZone(fake_id);
        CHECK(!removed);
        
        // Create zone and then try invalid operations
        auto& zone = error_farm.createField("Test Zone", createRectangle(0, 0, 100, 100));
        
        // Sample non-existent raster layer
        auto bad_sample = zone.sample_at("non_existent_layer", concord::Point(50, 50, 0));
        CHECK(!bad_sample.has_value());
        
        // Get non-existent raster layer
        auto bad_layer = zone.get_layer("non_existent_layer");
        CHECK(bad_layer == nullptr);
        
        CHECK(!zone.has_layer("non_existent_layer"));
    }
    
    SUBCASE("Boundary conditions") {
        Farm boundary_farm("Boundary Test Farm");
        
        // Create zone at origin
        auto& origin_zone = boundary_farm.createField("Origin", createRectangle(0, 0, 100, 100));
        
        // Test points exactly on boundaries
        auto on_corner = boundary_farm.findZonesContaining(concord::Point(0, 0, 0));
        auto on_edge = boundary_farm.findZonesContaining(concord::Point(50, 0, 0));
        auto on_opposite_corner = boundary_farm.findZonesContaining(concord::Point(100, 100, 0));
        
        // Results may vary based on polygon containment implementation
        // Main goal is to ensure no crashes
        
        // Test very small movements
        auto just_inside = boundary_farm.findZonesContaining(concord::Point(0.01, 0.01, 0));
        auto just_outside = boundary_farm.findZonesContaining(concord::Point(-0.01, -0.01, 0));
        
        CHECK(just_outside.size() == 0); // Should definitely be outside
    }
    
    SUBCASE("Empty farm operations") {
        Farm empty_farm("Empty Farm");
        
        // Test operations on empty farm
        CHECK(empty_farm.numZones() == 0);
        CHECK(!empty_farm.isValid());
        CHECK(empty_farm.totalArea() == 0.0);
        
        auto bbox = empty_farm.getBoundingBox();
        CHECK(!bbox.has_value());
        
        auto no_zones = empty_farm.findZonesContaining(concord::Point(0, 0, 0));
        CHECK(no_zones.size() == 0);
        
        auto no_radius_zones = empty_farm.findZonesWithinRadius(concord::Point(0, 0, 0), 1000.0);
        CHECK(no_radius_zones.size() == 0);
        
        auto no_nearest = empty_farm.findNearestZone(concord::Point(0, 0, 0));
        CHECK(no_nearest == nullptr);
        
        auto no_k_nearest = empty_farm.findKNearestZones(concord::Point(0, 0, 0), 5);
        CHECK(no_k_nearest.size() == 0);
        
        // Test spatial index on empty farm
        auto stats = empty_farm.getSpatialIndexStats();
        CHECK(stats.total_entries == 0);
    }
}

TEST_CASE("UUID system comprehensive test") {
    SUBCASE("UUID generation and uniqueness") {
        const int num_uuids = 10000;
        std::vector<UUID> uuids;
        uuids.reserve(num_uuids);
        
        // Generate many UUIDs
        for (int i = 0; i < num_uuids; ++i) {
            uuids.push_back(generateUUID());
        }
        
        // Check uniqueness
        std::set<std::string> uuid_strings;
        for (const auto& uuid : uuids) {
            std::string uuid_str = uuid.toString();
            CHECK(uuid_strings.find(uuid_str) == uuid_strings.end()); // Should be unique
            uuid_strings.insert(uuid_str);
        }
        
        CHECK(uuid_strings.size() == num_uuids);
        
        // Test round-trip conversion
        for (int i = 0; i < 100; ++i) {
            auto original = uuids[i];
            std::string str = original.toString();
            auto converted = uuidFromString(str);
            CHECK(original == converted);
        }
        
        // Test null UUID
        auto null_uuid = UUID::null();
        CHECK(null_uuid.isNull());
        CHECK(null_uuid.toString() == "00000000-0000-0000-0000-000000000000");
        
        // Test UUID in containers
        std::unordered_map<UUID, int, UUIDHash> uuid_map;
        for (int i = 0; i < 100; ++i) {
            uuid_map[uuids[i]] = i;
        }
        CHECK(uuid_map.size() == 100);
        
        // Verify lookups work
        for (int i = 0; i < 100; ++i) {
            CHECK(uuid_map[uuids[i]] == i);
        }
    }
}

TEST_CASE("Full system integration") {
    SUBCASE("Complete farm lifecycle") {
        // Create farm
        Farm lifecycle_farm("Lifecycle Test Farm");
        
        // Phase 1: Initial setup
        auto& field1 = lifecycle_farm.createField("Main Field", createRectangle(0, 0, 200, 200));
        auto& barn = lifecycle_farm.createBarn("Storage Barn", createRectangle(250, 50, 80, 60));
        
        field1.setProperty("crop_type", "wheat");
        field1.setProperty("planting_date", "2024-03-15");
        
        // Phase 2: Add infrastructure
        for (int i = 0; i < 10; ++i) {
            std::vector<concord::Point> row_points;
            double y = 20 + i * 16;
            row_points.emplace_back(10, y, 0);
            row_points.emplace_back(190, y, 0);
            field1.add_element(concord::Path(row_points), "crop_row");
        }
        
        // Phase 3: Add monitoring data
        concord::Grid<uint8_t> elevation_grid(20, 20, 10.0, true, concord::Pose{});
        concord::Grid<uint8_t> moisture_grid(20, 20, 10.0, true, concord::Pose{});
        
        for (size_t r = 0; r < 20; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
                moisture_grid.set_value(r, c, static_cast<uint8_t>(30 + (r * c) % 40));
            }
        }
        
        field1.add_layer("elevation", "terrain", elevation_grid, {{"units", "meters"}});
        field1.add_layer("soil_moisture", "environmental", moisture_grid, {{"units", "percentage"}});
        
        // Phase 4: Robot operations
        auto robot_id = generateUUID();
        field1.setOwnerRobot(robot_id);
        
        // Simulate robot working in field
        std::vector<concord::Point> robot_path = {
            concord::Point(50, 50, 0),
            concord::Point(100, 100, 0),
            concord::Point(150, 150, 0),
            concord::Point(100, 180, 0)
        };
        
        for (const auto& position : robot_path) {
            auto current_zones = lifecycle_farm.findZonesContaining(position);
            CHECK(current_zones.size() >= 1);
            
            if (current_zones[0]->has_layer("elevation")) {
                auto elevation = current_zones[0]->sample_at("elevation", position);
                CHECK(elevation.has_value());
            }
            
            if (current_zones[0]->has_layer("soil_moisture")) {
                auto moisture = current_zones[0]->sample_at("soil_moisture", position);
                CHECK(moisture.has_value());
            }
        }
        
        // Phase 5: Expansion
        auto& field2 = lifecycle_farm.createField("East Field", createRectangle(300, 0, 150, 150));
        field2.setProperty("crop_type", "corn");
        
        // Phase 6: Analysis
        CHECK(lifecycle_farm.numZones() == 3);
        CHECK(lifecycle_farm.totalArea() == doctest::Approx(40000.0 + 4800.0 + 22500.0));
        
        auto farm_bbox = lifecycle_farm.getBoundingBox();
        CHECK(farm_bbox.has_value());
        
        if (farm_bbox) {
            double farm_width = farm_bbox->max_point.x - farm_bbox->min_point.x;
            double farm_height = farm_bbox->max_point.y - farm_bbox->min_point.y;
            CHECK(farm_width == doctest::Approx(450.0));
            CHECK(farm_height == doctest::Approx(200.0));
        }
        
        // Phase 7: Cleanup
        field1.releaseOwnership();
        CHECK(!field1.hasOwner());
        
        // Test final state
        CHECK(lifecycle_farm.isValid());
        CHECK(field1.is_valid());
        CHECK(field2.is_valid());
        CHECK(barn.is_valid());
    }
}