#include "zoneout/zoneout.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Grid Center Position Example ===" << std::endl;
    std::cout << std::endl;
    
    // Create a 200m x 200m field
    concord::Polygon boundary;
    boundary.addPoint({0, 0, 0});
    boundary.addPoint({200, 0, 0});
    boundary.addPoint({200, 200, 0});
    boundary.addPoint({0, 200, 0});
    
    concord::Datum datum{52.0, 5.0, 0.0};
    double resolution = 20.0;  // 20m per cell -> 10x10 grid
    
    zoneout::Zone zone("my_field", "crop_field", boundary, datum, resolution);
    
    // Add a 10x10 grid layer
    concord::Grid<uint8_t> elevation(10, 10, resolution, true, concord::Pose{});
    
    for (size_t r = 0; r < 10; r++) {
        for (size_t c = 0; c < 10; c++) {
            elevation(r, c) = (r + c) * 10;
        }
    }
    
    zone.add_raster_layer(elevation, "elevation", "terrain");
    
    // Get the grid
    auto& raster = zone.raster_data();
    if (raster.hasGrids()) {
        auto& grid = raster.getGrid(0).grid;
        
        std::cout << std::fixed << std::setprecision(1);
        
        // Get center position of cell [5, 6]
        concord::Point center = grid.grid_to_world(5, 6);
        std::cout << "Cell [5,6] center: (" << center.x << ", " << center.y << ")" << std::endl;
        
        // Get value at that cell
        uint8_t value = grid(5, 6);
        std::cout << "Cell [5,6] value:  " << (int)value << std::endl;
        
        std::cout << std::endl;
        
        // Reverse: find which cell contains a position
        concord::Point robot_pos{100.0, 120.0, 0.0};
        auto [row, col] = grid.world_to_grid(robot_pos);
        
        std::cout << "Position (" << robot_pos.x << ", " << robot_pos.y 
                  << ") is in cell [" << row << "," << col << "]" << std::endl;
    }
    
    return 0;
}
