#include <iostream>
#include "include/zoneout/zoneout.hpp"

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

int main() {
    try {
        std::cout << "Creating farm..." << std::endl;
        Farm farm("Debug Farm");
        
        std::cout << "Creating zones..." << std::endl;
        auto& zone1 = farm.createField("Zone 1", createRectangle(0, 0, 100, 100));
        
        std::cout << "Zone 1 created with name: " << zone1.getName() << std::endl;
        std::cout << "Zone 1 has field boundary: " << zone1.hasFieldBoundary() << std::endl;
        
        if (zone1.hasFieldBoundary()) {
            auto boundary = zone1.getFieldBoundary();
            auto points = boundary.getPoints();
            std::cout << "Zone 1 has " << points.size() << " boundary points" << std::endl;
            for (size_t i = 0; i < points.size(); ++i) {
                std::cout << "  Point " << i << ": (" << points[i].x << ", " << points[i].y << ", " << points[i].z << ")" << std::endl;
            }
        }
        
        std::cout << "Testing point containment..." << std::endl;
        concord::Point test_point(50, 50, 0);
        
        if (zone1.hasFieldBoundary()) {
            auto boundary = zone1.getFieldBoundary();
            bool contains = boundary.contains(test_point);
            std::cout << "Zone 1 contains point (50, 50, 0): " << contains << std::endl;
        }
        
        std::cout << "Creating 100 zones for spatial index test..." << std::endl;
        Farm perf_farm("Performance Test Farm");
        
        const int grid_size = 10; // 10x10 = 100 zones
        const double zone_size = 100.0;
        
        int zones_with_boundary = 0;
        for (int i = 0; i < grid_size; ++i) {
            for (int j = 0; j < grid_size; ++j) {
                double x = i * zone_size;
                double y = j * zone_size;
                std::string name = "Zone_" + std::to_string(i) + "_" + std::to_string(j);
                auto& zone = perf_farm.createField(name, createRectangle(x, y, zone_size, zone_size));
                
                if (zone.hasFieldBoundary()) {
                    zones_with_boundary++;
                }
            }
        }
        
        std::cout << "Created " << perf_farm.numZones() << " zones" << std::endl;
        std::cout << "Zones with field boundary: " << zones_with_boundary << std::endl;
        
        auto stats = perf_farm.getSpatialIndexStats();
        std::cout << "Spatial index entries: " << stats.total_entries << std::endl;
        
        std::cout << "Testing radius search..." << std::endl;
        auto zones_within_10 = farm.findZonesWithinRadius(test_point, 10.0);
        std::cout << "Found " << zones_within_10.size() << " zones within radius 10.0" << std::endl;
        
        std::cout << "Debug completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}