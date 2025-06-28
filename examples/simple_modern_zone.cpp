#include <iostream>
#include "zoneout/zoneout.hpp"

int main() {
    // 1. Create zone boundary (100m x 50m rectangle)
    std::vector<concord::Point> points;
    points.emplace_back(0.0, 0.0, 0.0);    // bottom-left
    points.emplace_back(100.0, 0.0, 0.0);  // bottom-right  
    points.emplace_back(100.0, 50.0, 0.0); // top-right
    points.emplace_back(0.0, 50.0, 0.0);   // top-left
    
    concord::Polygon boundary(points);
    
    // 2. Create the zone using modern Vector/Raster system
    auto field = zoneout::Zone::createField("My Field", boundary);
    
    // 3. Add ONE raster layer (elevation data - 10x20 grid, 5m resolution)
    concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
    
    // Fill with sample elevation data (100-119 range)
    for (size_t row = 0; row < 10; ++row) {
        for (size_t col = 0; col < 20; ++col) {
            uint8_t elevation = 100 + (row * col / 10);
            elevation_grid.set_value(row, col, elevation);
        }
    }
    
    // Add elevation layer to zone
    field.addElevationLayer(elevation_grid, "meters");
    
    // Done! Zone with one layer created using modern system
    std::cout << "Modern Zone created:" << std::endl;
    std::cout << "Name: " << field.getName() << std::endl;
    std::cout << "Type: " << field.getType() << std::endl;
    std::cout << "Area: " << field.area() << " m²" << std::endl;
    std::cout << "Raster layers: " << field.numRasterLayers() << std::endl;
    
    // Query elevation at a point
    concord::Point query_point(25.0, 30.0, 0.0);
    auto elevation = field.sampleRasterAt("elevation", query_point);
    if (elevation) {
        std::cout << "Elevation at (25,30): " << static_cast<int>(*elevation) << " meters" << std::endl;
    }
    
    return 0;
}