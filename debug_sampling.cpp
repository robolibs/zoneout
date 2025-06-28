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
        std::cout << "=== Debug Raster Sampling ===" << std::endl;
        
        // Create a field like in the integration test
        auto boundary = createRectangle(0, 0, 500, 300);  // North Field: 500m x 300m
        Zone north_field("North Field", "field", boundary);
        
        std::cout << "Created North Field: " << north_field.area() << " m²" << std::endl;
        
        // Add elevation data exactly like in integration test
        // Grid should cover the field area (0,0 to 500,300), so center it at (250,150)
        concord::Pose grid_pose;
        grid_pose.point = concord::Point(250, 150, 0); // Center of the field
        
        std::cout << "Grid pose: (" << grid_pose.point.x << ", " << grid_pose.point.y << ", " << grid_pose.point.z << ")" << std::endl;
        
        concord::Grid<uint8_t> elevation_grid(25, 50, 10.0, true, grid_pose);
        std::cout << "Created elevation grid: " << elevation_grid.rows() << " x " << elevation_grid.cols() 
                  << " with resolution " << elevation_grid.inradius() << std::endl;
                  
        // Fill grid with test data
        for (size_t r = 0; r < 25; ++r) {
            for (size_t c = 0; c < 50; ++c) {
                // Simulate gentle slope from 95m to 105m elevation
                uint8_t elevation = static_cast<uint8_t>(95 + (r + c) * 10 / (25 + 50));
                elevation_grid.set_value(r, c, elevation);
            }
        }
        
        std::cout << "Filled elevation grid" << std::endl;
        std::cout << "Sample grid values:" << std::endl;
        std::cout << "  (0,0): " << static_cast<int>(elevation_grid(0, 0).second) << std::endl;
        std::cout << "  (10,20): " << static_cast<int>(elevation_grid(10, 20).second) << std::endl;
        std::cout << "  (24,49): " << static_cast<int>(elevation_grid(24, 49).second) << std::endl;
        
        // Add to zone
        north_field.addElevationLayer(elevation_grid, "meters");
        std::cout << "Added elevation layer to zone" << std::endl;
        
        // Check if layer exists
        bool has_layer = north_field.hasRasterLayer("elevation");
        std::cout << "Zone has elevation layer: " << has_layer << std::endl;
        
        if (has_layer) {
            auto layer = north_field.getRasterLayer("elevation");
            if (layer) {
                std::cout << "Layer grid size: " << layer->grid.rows() << " x " << layer->grid.cols() << std::endl;
                std::cout << "Layer grid resolution: " << layer->grid.inradius() << std::endl;
                
                // Check grid corners
                auto corners = layer->grid.corners();
                std::cout << "Grid corners:" << std::endl;
                for (int i = 0; i < 4; ++i) {
                    std::cout << "  Corner " << i << ": (" << corners[i].x << ", " << corners[i].y << ", " << corners[i].z << ")" << std::endl;
                }
            }
        }
        
        // Test sampling at robot position like in integration test
        concord::Point robot_position(250, 150, 0);
        std::cout << "\nTesting sampling at robot position (" << robot_position.x << ", " << robot_position.y << ", " << robot_position.z << ")" << std::endl;
        
        auto elevation = north_field.sampleRasterAt("elevation", robot_position);
        std::cout << "Sampled elevation: ";
        if (elevation.has_value()) {
            std::cout << static_cast<int>(*elevation) << std::endl;
        } else {
            std::cout << "NO VALUE" << std::endl;
        }
        
        // Test other positions
        std::vector<concord::Point> test_points = {
            concord::Point(0, 0, 0),
            concord::Point(50, 50, 0),
            concord::Point(100, 100, 0),
            concord::Point(500, 300, 0)
        };
        
        for (const auto& point : test_points) {
            auto sample = north_field.sampleRasterAt("elevation", point);
            std::cout << "Point (" << point.x << ", " << point.y << "): ";
            if (sample.has_value()) {
                std::cout << static_cast<int>(*sample) << std::endl;
            } else {
                std::cout << "NO VALUE" << std::endl;
            }
        }
        
        std::cout << "\nDebug completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}