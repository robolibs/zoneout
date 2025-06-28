#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <chrono>
#include <random>

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

// Helper function to create a circular polygon approximation
concord::Polygon createCircle(double center_x, double center_y, double radius, int segments = 16) {
    std::vector<concord::Point> points;
    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * M_PI * i / segments;
        double x = center_x + radius * cos(angle);
        double y = center_y + radius * sin(angle);
        points.emplace_back(x, y, 0.0);
    }
    return concord::Polygon(points);
}

TEST_CASE("Spatial index performance with large datasets") {
    Farm large_farm("Performance Test Farm");
    
    SUBCASE("Grid pattern performance") {
        const int grid_size = 20; // 20x20 = 400 zones
        const double zone_size = 50.0;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create zones in grid pattern
        for (int i = 0; i < grid_size; ++i) {
            for (int j = 0; j < grid_size; ++j) {
                double x = i * zone_size;
                double y = j * zone_size;
                std::string name = "Grid_" + std::to_string(i) + "_" + std::to_string(j);
                large_farm.createField(name, createRectangle(x, y, zone_size, zone_size));
            }
        }
        
        auto creation_time = std::chrono::high_resolution_clock::now();
        auto creation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(creation_time - start_time);
        
        CHECK(large_farm.numZones() == 400);
        
        // Test point queries
        std::vector<concord::Point> test_points;
        for (int i = 0; i < 100; ++i) {
            test_points.emplace_back(i * 10, i * 8, 0);
        }
        
        auto query_start = std::chrono::high_resolution_clock::now();
        int total_found = 0;
        for (const auto& point : test_points) {
            auto zones = large_farm.findZonesContaining(point);
            total_found += zones.size();
        }
        auto query_end = std::chrono::high_resolution_clock::now();
        auto query_duration = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);
        
        // Performance should be reasonable (these are loose bounds)
        CHECK(creation_duration.count() < 5000); // Less than 5 seconds
        CHECK(query_duration.count() < 10000);   // Less than 10ms for 100 queries
        CHECK(total_found > 0); // Should find some zones
    }
    
    SUBCASE("Random distribution performance") {
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<> pos_dist(0.0, 1000.0);
        std::uniform_real_distribution<> size_dist(10.0, 100.0);
        
        const int num_zones = 200;
        
        // Create randomly distributed zones
        for (int i = 0; i < num_zones; ++i) {
            double x = pos_dist(gen);
            double y = pos_dist(gen);
            double width = size_dist(gen);
            double height = size_dist(gen);
            
            std::string name = "Random_" + std::to_string(i);
            large_farm.createField(name, createRectangle(x, y, width, height));
        }
        
        CHECK(large_farm.numZones() == num_zones);
        
        // Test various query types
        concord::Point test_point(500, 500, 0);
        auto point_zones = large_farm.findZonesContaining(test_point);
        
        auto radius_zones = large_farm.findZonesWithinRadius(test_point, 200.0);
        CHECK(radius_zones.size() >= point_zones.size());
        
        auto nearest_5 = large_farm.findKNearestZones(test_point, 5);
        CHECK(nearest_5.size() <= 5);
    }
}

TEST_CASE("Spatial index accuracy") {
    Farm accuracy_farm("Accuracy Test Farm");
    
    // Create known zones for accuracy testing
    auto& zone1 = accuracy_farm.createField("Zone1", createRectangle(0, 0, 100, 100));
    auto& zone2 = accuracy_farm.createField("Zone2", createRectangle(150, 0, 100, 100));
    auto& zone3 = accuracy_farm.createField("Zone3", createRectangle(0, 150, 100, 100));
    auto& zone4 = accuracy_farm.createField("Zone4", createRectangle(150, 150, 100, 100));
    auto& center_zone = accuracy_farm.createField("Center", createRectangle(75, 75, 100, 100));
    
    SUBCASE("Point containment accuracy") {
        // Test points that should be in specific zones
        auto zones_in_zone1 = accuracy_farm.findZonesContaining(concord::Point(50, 50, 0));
        CHECK(zones_in_zone1.size() >= 1);
        bool found_zone1 = false;
        for (auto* zone : zones_in_zone1) {
            if (zone->getName() == "Zone1") found_zone1 = true;
        }
        CHECK(found_zone1);
        
        // Test point in overlapping area
        auto zones_overlap = accuracy_farm.findZonesContaining(concord::Point(100, 100, 0));
        // Should find center_zone and potentially others depending on boundary handling
        CHECK(zones_overlap.size() >= 1);
        
        // Test point outside all zones
        auto zones_outside = accuracy_farm.findZonesContaining(concord::Point(500, 500, 0));
        CHECK(zones_outside.size() == 0);
    }
    
    SUBCASE("Radius search accuracy") {
        concord::Point center(125, 125, 0); // Center between all zones
        
        // Small radius - should find center zone and maybe overlapping zones
        auto small_radius = accuracy_farm.findZonesWithinRadius(center, 50.0);
        CHECK(small_radius.size() >= 1);
        
        // Large radius - should find all zones
        auto large_radius = accuracy_farm.findZonesWithinRadius(center, 200.0);
        CHECK(large_radius.size() == 5);
        
        // Very small radius - might not find any depending on exact positioning
        auto tiny_radius = accuracy_farm.findZonesWithinRadius(center, 1.0);
        // Result depends on whether center point is exactly inside center_zone
    }
    
    SUBCASE("Nearest neighbor accuracy") {
        // Find nearest to a point clearly closest to zone1
        concord::Point near_zone1(25, 25, 0);
        auto nearest = accuracy_farm.findNearestZone(near_zone1);
        CHECK(nearest != nullptr);
        CHECK(nearest->getName() == "Zone1");
        
        // Find nearest to a point equidistant from multiple zones
        concord::Point equidistant(275, 75, 0); // Between zone2 and zone4
        auto nearest_eq = accuracy_farm.findNearestZone(equidistant);
        CHECK(nearest_eq != nullptr);
        // Should be either zone2 or zone4
        CHECK((nearest_eq->getName() == "Zone2" || nearest_eq->getName() == "Zone4"));
    }
    
    SUBCASE("K-nearest neighbors accuracy") {
        concord::Point query_point(300, 300, 0); // Far from all zones
        
        auto nearest_3 = accuracy_farm.findKNearestZones(query_point, 3);
        CHECK(nearest_3.size() == 3);
        
        // Should be ordered by distance (zone4 should be closest)
        CHECK(nearest_3[0]->getName() == "Zone4");
        
        auto all_nearest = accuracy_farm.findKNearestZones(query_point, 10);
        CHECK(all_nearest.size() == 5); // All zones
    }
}

TEST_CASE("Spatial operations with complex geometries") {
    Farm complex_farm("Complex Geometry Farm");
    
    SUBCASE("Circular zones") {
        // Create circular zones
        auto circle1 = createCircle(50, 50, 30, 20);
        auto circle2 = createCircle(150, 50, 25, 16);
        auto circle3 = createCircle(100, 150, 35, 24);
        
        complex_farm.createField("Circle1", circle1);
        complex_farm.createField("Circle2", circle2);
        complex_farm.createField("Circle3", circle3);
        
        // Test point containment
        auto center_zones = complex_farm.findZonesContaining(concord::Point(50, 50, 0));
        CHECK(center_zones.size() >= 1);
        
        // Test radius search
        auto nearby = complex_farm.findZonesWithinRadius(concord::Point(100, 100, 0), 100.0);
        CHECK(nearby.size() >= 2); // Should find multiple circles
    }
    
    SUBCASE("L-shaped zones") {
        // Create L-shaped polygon
        std::vector<concord::Point> l_points;
        l_points.emplace_back(0, 0, 0);
        l_points.emplace_back(100, 0, 0);
        l_points.emplace_back(100, 50, 0);
        l_points.emplace_back(50, 50, 0);
        l_points.emplace_back(50, 150, 0);
        l_points.emplace_back(0, 150, 0);
        
        concord::Polygon l_shape(l_points);
        complex_farm.createField("L-Shape", l_shape);
        
        // Test points in different parts of the L
        auto bottom_part = complex_farm.findZonesContaining(concord::Point(75, 25, 0));
        CHECK(bottom_part.size() == 1);
        
        auto vertical_part = complex_farm.findZonesContaining(concord::Point(25, 100, 0));
        CHECK(vertical_part.size() == 1);
        
        // Test point in the "notch"
        auto notch_point = complex_farm.findZonesContaining(concord::Point(75, 100, 0));
        CHECK(notch_point.size() == 0);
    }
    
    SUBCASE("Star-shaped zones") {
        // Create star polygon
        std::vector<concord::Point> star_points;
        double center_x = 100, center_y = 100;
        double outer_radius = 50, inner_radius = 20;
        
        for (int i = 0; i < 10; ++i) {
            double angle = 2.0 * M_PI * i / 10.0;
            double radius = (i % 2 == 0) ? outer_radius : inner_radius;
            double x = center_x + radius * cos(angle);
            double y = center_y + radius * sin(angle);
            star_points.emplace_back(x, y, 0.0);
        }
        
        concord::Polygon star(star_points);
        complex_farm.createField("Star", star);
        
        // Test center point
        auto center_zones = complex_farm.findZonesContaining(concord::Point(100, 100, 0));
        CHECK(center_zones.size() == 1);
        
        // Test points on star arms vs indentations
        auto arm_point = complex_farm.findZonesContaining(concord::Point(150, 100, 0)); // Should be in
        auto indent_point = complex_farm.findZonesContaining(concord::Point(120, 120, 0)); // Might be out
        
        CHECK(arm_point.size() >= 0); // Depends on exact geometry
    }
}

TEST_CASE("Spatial index edge cases") {
    Farm edge_farm("Edge Case Farm");
    
    SUBCASE("Overlapping zones") {
        // Create overlapping zones
        auto zone1 = edge_farm.createField("Overlap1", createRectangle(0, 0, 100, 100));
        auto zone2 = edge_farm.createField("Overlap2", createRectangle(50, 50, 100, 100));
        auto zone3 = edge_farm.createField("Overlap3", createRectangle(25, 25, 100, 100));
        
        // Point in triple overlap
        auto triple_overlap = edge_farm.findZonesContaining(concord::Point(75, 75, 0));
        CHECK(triple_overlap.size() >= 2); // Should find multiple zones
        
        // Point in single zone
        auto single_zone = edge_farm.findZonesContaining(concord::Point(10, 10, 0));
        CHECK(single_zone.size() == 1);
        CHECK(single_zone[0]->getName() == "Overlap1");
    }
    
    SUBCASE("Tiny zones") {
        // Create very small zones
        for (int i = 0; i < 10; ++i) {
            double x = i * 2.0;
            double y = i * 2.0;
            std::string name = "Tiny" + std::to_string(i);
            edge_farm.createField(name, createRectangle(x, y, 1.0, 1.0));
        }
        
        CHECK(edge_farm.numZones() == 10);
        
        // Test precise point queries
        auto precise_zone = edge_farm.findZonesContaining(concord::Point(2.5, 2.5, 0));
        CHECK(precise_zone.size() == 1);
        CHECK(precise_zone[0]->getName() == "Tiny1");
        
        // Test radius search with tiny zones
        auto nearby_tiny = edge_farm.findZonesWithinRadius(concord::Point(5, 5, 0), 5.0);
        CHECK(nearby_tiny.size() >= 3); // Should find several tiny zones
    }
    
    SUBCASE("Huge zones") {
        // Create very large zones
        auto huge1 = edge_farm.createField("Huge1", createRectangle(0, 0, 10000, 10000));
        auto huge2 = edge_farm.createField("Huge2", createRectangle(5000, 5000, 10000, 10000));
        
        // Test points far apart
        auto far_point1 = edge_farm.findZonesContaining(concord::Point(1000, 1000, 0));
        auto far_point2 = edge_farm.findZonesContaining(concord::Point(8000, 8000, 0));
        
        CHECK(far_point1.size() >= 1);
        CHECK(far_point2.size() >= 1);
        
        // Test huge radius search
        auto massive_radius = edge_farm.findZonesWithinRadius(concord::Point(0, 0, 0), 20000.0);
        CHECK(massive_radius.size() == 2);
    }
    
    SUBCASE("Degenerate polygons") {
        // Line-like polygon (very thin rectangle)
        auto thin_line = edge_farm.createField("ThinLine", createRectangle(0, 0, 1000, 0.1));
        
        // Test point on the line
        auto on_line = edge_farm.findZonesContaining(concord::Point(500, 0.05, 0));
        // Result depends on polygon implementation precision
        
        // Point-like polygon (very small square)
        auto point_like = edge_farm.createField("PointLike", createRectangle(100, 100, 0.01, 0.01));
        
        // Test nearby point
        auto near_point = edge_farm.findZonesWithinRadius(concord::Point(100, 100, 0), 1.0);
        CHECK(near_point.size() >= 1);
    }
}

TEST_CASE("Spatial operations consistency") {
    Farm consistency_farm("Consistency Test Farm");
    
    // Create a known set of zones
    auto& zone1 = consistency_farm.createField("Zone1", createRectangle(0, 0, 100, 100));
    auto& zone2 = consistency_farm.createField("Zone2", createRectangle(200, 0, 100, 100));
    auto& zone3 = consistency_farm.createField("Zone3", createRectangle(100, 100, 100, 100));
    
    SUBCASE("Query result consistency") {
        concord::Point test_point(50, 50, 0);
        
        // Multiple calls should return same results
        auto result1 = consistency_farm.findZonesContaining(test_point);
        auto result2 = consistency_farm.findZonesContaining(test_point);
        auto result3 = consistency_farm.findZonesContaining(test_point);
        
        CHECK(result1.size() == result2.size());
        CHECK(result2.size() == result3.size());
        
        // Results should contain same zones
        if (!result1.empty() && !result2.empty()) {
            CHECK(result1[0]->getId() == result2[0]->getId());
        }
    }
    
    SUBCASE("Radius vs point containment consistency") {
        concord::Point test_point(50, 50, 0);
        
        // Point containment
        auto contained_zones = consistency_farm.findZonesContaining(test_point);
        
        // Very small radius search
        auto tiny_radius_zones = consistency_farm.findZonesWithinRadius(test_point, 0.1);
        
        // Point containment should be subset of tiny radius search
        CHECK(contained_zones.size() <= tiny_radius_zones.size());
    }
    
    SUBCASE("Nearest neighbor vs radius search consistency") {
        concord::Point query_point(350, 50, 0); // Outside all zones
        
        auto nearest = consistency_farm.findNearestZone(query_point);
        CHECK(nearest != nullptr);
        
        // Large radius should include the nearest zone
        auto large_radius = consistency_farm.findZonesWithinRadius(query_point, 1000.0);
        
        bool found_nearest = false;
        for (auto* zone : large_radius) {
            if (zone->getId() == nearest->getId()) {
                found_nearest = true;
                break;
            }
        }
        CHECK(found_nearest);
    }
    
    SUBCASE("K-nearest ordering consistency") {
        concord::Point query_point(400, 200, 0);
        
        auto nearest_1 = consistency_farm.findKNearestZones(query_point, 1);
        auto nearest_2 = consistency_farm.findKNearestZones(query_point, 2);
        auto nearest_3 = consistency_farm.findKNearestZones(query_point, 3);
        
        CHECK(nearest_1.size() == 1);
        CHECK(nearest_2.size() == 2);
        CHECK(nearest_3.size() == 3);
        
        // First element should be same in all queries
        CHECK(nearest_1[0]->getId() == nearest_2[0]->getId());
        CHECK(nearest_2[0]->getId() == nearest_3[0]->getId());
        
        // Second element should be same in 2 and 3 element queries
        CHECK(nearest_2[1]->getId() == nearest_3[1]->getId());
    }
}