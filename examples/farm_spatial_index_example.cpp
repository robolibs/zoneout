#include <iostream>
#include <chrono>
#include "zoneout/zoneout.hpp"

// Helper function to create a rectangle
concord::Polygon createRectangle(double x, double y, double width, double height) {
    std::vector<concord::Point> points;
    points.emplace_back(x, y, 0.0);
    points.emplace_back(x + width, y, 0.0);
    points.emplace_back(x + width, y + height, 0.0);
    points.emplace_back(x, y + height, 0.0);
    return concord::Polygon(points);
}

// Helper function to measure execution time
template<typename Func>
double measureExecutionTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0; // Return milliseconds
}

int main() {
    std::cout << "Farm Spatial Index Performance Example\n" << std::endl;
    
    // ========== Create Large Farm with Many Zones ==========
    zoneout::Farm large_farm("Large Agricultural Farm");
    
    std::cout << "=== Creating Large Farm with 1000+ Zones ===" << std::endl;
    
    // Create a grid of zones to simulate a large agricultural area
    const int grid_size = 32; // 32x32 = 1024 zones
    const double zone_size = 100.0; // 100m x 100m zones
    
    auto creation_time = measureExecutionTime([&]() {
        for (int i = 0; i < grid_size; ++i) {
            for (int j = 0; j < grid_size; ++j) {
                double x = i * zone_size;
                double y = j * zone_size;
                
                std::string zone_name = "Zone_" + std::to_string(i) + "_" + std::to_string(j);
                std::string zone_type = (i + j) % 3 == 0 ? "field" : 
                                       (i + j) % 3 == 1 ? "pasture" : "forest";
                
                auto& zone = large_farm.createZone(zone_name, zone_type, 
                                                 createRectangle(x, y, zone_size, zone_size));
                
                // Add some properties
                zone.setProperty("grid_x", std::to_string(i));
                zone.setProperty("grid_y", std::to_string(j));
                zone.setProperty("area_hectares", std::to_string(zone_size * zone_size / 10000.0));
            }
        }
    });
    
    std::cout << "Created " << large_farm.numZones() << " zones in " 
              << creation_time << " ms" << std::endl;
    
    // ========== Spatial Index Statistics ==========
    std::cout << "\n=== Spatial Index Statistics ===" << std::endl;
    
    auto stats = large_farm.getSpatialIndexStats();
    std::cout << "Spatial index entries: " << stats.total_entries << std::endl;
    std::cout << "Tree height: " << stats.tree_height << std::endl;
    
    // ========== Performance Comparison: Point Query ==========
    std::cout << "\n=== Point Query Performance Test ===" << std::endl;
    
    concord::Point test_point(1550.0, 1550.0, 0.0); // Middle of the farm
    std::vector<zoneout::Zone*> point_results;
    
    // Measure spatial index performance
    auto spatial_time = measureExecutionTime([&]() {
        for (int i = 0; i < 1000; ++i) {
            point_results = large_farm.findZonesContaining(test_point);
        }
    });
    
    std::cout << "1000 point queries with spatial index: " << spatial_time << " ms" << std::endl;
    std::cout << "Found " << point_results.size() << " zones containing point (" 
              << test_point.x << ", " << test_point.y << ")" << std::endl;
    
    // ========== Radius Search Performance ==========
    std::cout << "\n=== Radius Search Performance Test ===" << std::endl;
    
    concord::Point search_center(1600.0, 1600.0, 0.0);
    double search_radius = 300.0; // 300m radius
    
    std::vector<zoneout::Zone*> radius_results;
    auto radius_time = measureExecutionTime([&]() {
        for (int i = 0; i < 100; ++i) {
            radius_results = large_farm.findZonesWithinRadius(search_center, search_radius);
        }
    });
    
    std::cout << "100 radius searches (" << search_radius << "m): " << radius_time << " ms" << std::endl;
    std::cout << "Found " << radius_results.size() << " zones within radius" << std::endl;
    
    // ========== K-Nearest Neighbors Test ==========
    std::cout << "\n=== K-Nearest Neighbors Test ===" << std::endl;
    
    concord::Point knn_point(2000.0, 2000.0, 0.0);
    size_t k = 10;
    
    auto knn_results = large_farm.findKNearestZones(knn_point, k);
    std::cout << "Found " << knn_results.size() << " nearest zones to point (" 
              << knn_point.x << ", " << knn_point.y << ")" << std::endl;
    
    for (size_t i = 0; i < knn_results.size(); ++i) {
        std::cout << "  " << (i+1) << ". " << knn_results[i]->getName() 
                  << " (" << knn_results[i]->getType() << ")" << std::endl;
    }
    
    // ========== Bounding Box Query Test ==========
    std::cout << "\n=== Bounding Box Query Test ===" << std::endl;
    
    concord::AABB query_bounds(concord::Point(500.0, 500.0, 0.0),
                              concord::Point(1500.0, 1500.0, 0.0));
    
    auto bounds_results = large_farm.findZonesInBounds(query_bounds);
    std::cout << "Found " << bounds_results.size() << " zones in bounding box" << std::endl;
    
    // ========== Polygon Intersection Test ==========
    std::cout << "\n=== Polygon Intersection Test ===" << std::endl;
    
    // Create a large polygon that crosses multiple zones
    auto query_polygon = createRectangle(800.0, 800.0, 600.0, 600.0);
    
    std::vector<zoneout::Zone*> intersection_results;
    auto intersection_time = measureExecutionTime([&]() {
        for (int i = 0; i < 100; ++i) {
            intersection_results = large_farm.findZonesIntersecting(query_polygon);
        }
    });
    
    std::cout << "100 polygon intersection queries: " << intersection_time << " ms" << std::endl;
    std::cout << "Found " << intersection_results.size() << " zones intersecting with query polygon" << std::endl;
    
    // ========== Zone Type Distribution ==========
    std::cout << "\n=== Zone Type Distribution ===" << std::endl;
    
    std::cout << "Field zones: " << large_farm.numZonesByType("field") << std::endl;
    std::cout << "Pasture zones: " << large_farm.numZonesByType("pasture") << std::endl;
    std::cout << "Forest zones: " << large_farm.numZonesByType("forest") << std::endl;
    
    // ========== Farm Area Statistics ==========
    std::cout << "\n=== Farm Area Statistics ===" << std::endl;
    
    std::cout << "Total farm area: " << large_farm.totalArea() / 10000.0 << " hectares" << std::endl;
    std::cout << "Field area: " << large_farm.areaByType("field") / 10000.0 << " hectares" << std::endl;
    std::cout << "Pasture area: " << large_farm.areaByType("pasture") / 10000.0 << " hectares" << std::endl;
    std::cout << "Forest area: " << large_farm.areaByType("forest") / 10000.0 << " hectares" << std::endl;
    
    auto bounding_box = large_farm.getBoundingBox();
    if (bounding_box) {
        std::cout << "Farm bounding box: " 
                  << "(" << bounding_box->min_point.x << ", " << bounding_box->min_point.y << ") to "
                  << "(" << bounding_box->max_point.x << ", " << bounding_box->max_point.y << ")" << std::endl;
        
        double box_width = bounding_box->max_point.x - bounding_box->min_point.x;
        double box_height = bounding_box->max_point.y - bounding_box->min_point.y;
        std::cout << "Farm dimensions: " << box_width << "m x " << box_height << "m" << std::endl;
    }
    
    // ========== Memory and Performance Summary ==========
    std::cout << "\n=== Performance Summary ===" << std::endl;
    
    std::cout << "Spatial indexing provides significant performance improvements for:" << std::endl;
    std::cout << "- Point-in-polygon queries (O(log n) vs O(n))" << std::endl;
    std::cout << "- Radius searches (spatial pruning vs full scan)" << std::endl;
    std::cout << "- Bounding box queries (R-tree traversal vs linear search)" << std::endl;
    std::cout << "- Polygon intersection tests (candidate filtering)" << std::endl;
    
    std::cout << "\nWith " << large_farm.numZones() << " zones, spatial indexing reduces" << std::endl;
    std::cout << "query time from O(" << large_farm.numZones() << ") to O(log " 
              << large_farm.numZones() << ") + candidates" << std::endl;
    
    std::cout << "\n=== Spatial Index Example Complete! ===" << std::endl;
    return 0;
}