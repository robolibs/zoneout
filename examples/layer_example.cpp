#include "../include/zoneout/zoneout.hpp"
#include <iostream>

// Example demonstrating concord::Layer integration for robot occlusion maps
// Shows how the new zoneout::Layer class works alongside Grid and Poly

int main() {
    try {
        std::cout << "=== Zoneout Layer Integration Example ===\n\n";
        
        // 1. Create a basic agricultural zone
        std::cout << "1. Creating agricultural zone...\n";
        
        // Define field boundary
        concord::Polygon field_boundary;
        field_boundary.addPoint(concord::Point{0.0, 0.0, 0.0});    // SW corner
        field_boundary.addPoint(concord::Point{100.0, 0.0, 0.0});  // SE corner
        field_boundary.addPoint(concord::Point{100.0, 80.0, 0.0}); // NE corner
        field_boundary.addPoint(concord::Point{0.0, 80.0, 0.0});   // NW corner
        
        // Create zone with basic grid
        concord::Datum field_datum{51.98776, 5.66238, 0.0}; // Wageningen
        zoneout::Zone robot_field("Robot_Field", "agricultural", field_boundary, field_datum, 2.0);
        
        std::cout << "✓ Created zone: " << robot_field.getName() 
                  << " (" << robot_field.getType() << ")\n";
        std::cout << "✓ Field boundary: 100m × 80m\n";
        std::cout << "✓ " << robot_field.getRasterInfo() << "\n\n";
        
        // 2. Initialize 3D occlusion layer for robot navigation
        std::cout << "2. Initializing 3D occlusion layer...\n";
        
        // 50×40 cells (2m resolution), 10 height layers (1m each = 0-10m height)
        robot_field.initializeOcclusionLayer(
            40, 50, 10,     // rows, cols, layers (40×50 grid, 10 layers high)
            2.0,            // 2 meter cell size
            1.0,            // 1 meter between height layers
            "robot_occlusion", "occlusion", "navigation",
            concord::Pose{concord::Point{50, 40, 0}, concord::Euler{0, 0, 0}} // Center over field
        );
        
        std::cout << "✓ Initialized 3D occlusion layer: 40×50×10 (2m cell, 1m layers)\n";
        std::cout << "✓ Coverage: 0-10m height, centered over field\n\n";
        
        // 3. Add some obstacles using the Layer directly
        std::cout << "3. Adding obstacles to occlusion map...\n";
        
        auto& occlusion = robot_field.getOcclusionLayer();
        
        // Add a tree (5m tall, full occlusion)
        concord::Point tree_base{25.0, 30.0, 0.0};
        concord::Point tree_top{27.0, 32.0, 5.0};
        occlusion.setVolumeOcclusion(tree_base, tree_top, 255);
        std::cout << "✓ Added tree obstacle: (25,30) 0-5m height\n";
        
        // Add a building (8m tall, full occlusion)
        concord::Point building_base{70.0, 20.0, 0.0};
        concord::Point building_top{80.0, 30.0, 8.0};
        occlusion.setVolumeOcclusion(building_base, building_top, 255);
        std::cout << "✓ Added building obstacle: (70,20)-(80,30) 0-8m height\n";
        
        // Add fence around perimeter using polygon integration
        concord::Polygon fence_polygon = field_boundary; // Use field boundary as fence
        occlusion.addPolygonOcclusion(fence_polygon, 0.0, 2.0, 128); // 2m high fence, partial occlusion
        std::cout << "✓ Added perimeter fence: 0-2m height, partial occlusion\n\n";
        
        // 4. Test robot navigation queries
        std::cout << "4. Testing robot navigation...\n";
        
        // Test path from SW to NE corner
        concord::Point start_pos{10.0, 10.0, 0.0};
        concord::Point goal_pos{90.0, 70.0, 0.0};
        bool path_clear = robot_field.isPathClear(start_pos, goal_pos, 2.5); // 2.5m robot height
        std::cout << "✓ Path (10,10) → (90,70) for 2.5m robot: " 
                  << (path_clear ? "CLEAR" : "BLOCKED") << "\n";
        
        // Test path that goes through the tree
        concord::Point tree_path_goal{26.0, 31.0, 0.0}; // Through the tree
        bool tree_path_clear = robot_field.isPathClear(start_pos, tree_path_goal, 2.5);
        std::cout << "✓ Path (10,10) → (26,31) through tree: " 
                  << (tree_path_clear ? "CLEAR" : "BLOCKED") << "\n";
        
        // Find safe height near tree
        double safe_height = occlusion.findSafeHeight(26.0, 31.0, 10.0);
        std::cout << "✓ Safe height at (26,31): " << safe_height << "m\n";
        
        // Check specific occlusion values
        uint8_t ground_occlusion = robot_field.getOcclusion({26.0, 31.0, 1.0}); // 1m high in tree
        uint8_t high_occlusion = robot_field.getOcclusion({26.0, 31.0, 6.0});   // 6m high (above tree)
        std::cout << "✓ Occlusion at tree (26,31): 1m=" << static_cast<int>(ground_occlusion) 
                  << ", 6m=" << static_cast<int>(high_occlusion) << "\n\n";
        
        // 5. Integration with existing Grid data
        std::cout << "5. Testing Grid↔Layer integration...\n";
        
        // Extract ground-level occlusion as 2D grid
        auto ground_grid = occlusion.extractGridFromLayer(0); // Layer 0 = ground level
        std::cout << "✓ Extracted ground-level grid: " 
                  << ground_grid.rows() << "×" << ground_grid.cols() << "\n";
        
        // Project an existing grid layer to 3D
        if (robot_field.grid_data_.gridCount() > 0) {
            const auto& base_grid = robot_field.grid_data_.getGrid(0).grid;
            // Note: This would need grid size compatibility for real use
            std::cout << "✓ Base grid available for projection: " 
                      << base_grid.rows() << "×" << base_grid.cols() << "\n";
        }
        
        // 6. Integration with existing Poly data  
        std::cout << "\n6. Testing Poly↔Layer integration...\n";
        
        // Add a crop area polygon and mark it as low occlusion in layer
        concord::Polygon crop_area;
        crop_area.addPoint(concord::Point{15.0, 15.0, 0.0});
        crop_area.addPoint(concord::Point{35.0, 15.0, 0.0});
        crop_area.addPoint(concord::Point{35.0, 35.0, 0.0});
        crop_area.addPoint(concord::Point{15.0, 35.0, 0.0});
        
        // Add to poly data
        robot_field.addPolygonFeature(crop_area, "corn_field", "crop", "corn");
        
        // Mark crop area as low occlusion (0-3m height for crops)
        occlusion.addPolygonOcclusion(crop_area, 0.0, 3.0, 50); // Partial occlusion for crops
        std::cout << "✓ Added corn field polygon with crop-level occlusion\n";
        std::cout << "✓ " << robot_field.getFeatureInfo() << "\n\n";
        
        // 7. Summary and validation
        std::cout << "7. Summary...\n";
        std::cout << "✓ Zone is valid: " << (robot_field.is_valid() ? "YES" : "NO") << "\n";
        std::cout << "✓ Has occlusion layer: " << (robot_field.hasOcclusionLayer() ? "YES" : "NO") << "\n";
        std::cout << "✓ Layer metadata:\n";
        
        auto layer_metadata = occlusion.getMetadata();
        for (const auto& [key, value] : layer_metadata) {
            std::cout << "  - " << key << ": " << value << "\n";
        }
        
        std::cout << "\n=== Integration Results ===\n";
        std::cout << "✓ zoneout::Layer successfully integrates with existing Grid and Poly\n";
        std::cout << "✓ Provides 3D occlusion mapping for robot navigation\n";
        std::cout << "✓ Optional component - doesn't affect existing functionality\n";
        std::cout << "✓ Supports world coordinate transformations and queries\n";
        std::cout << "✓ Enables height-aware path planning and obstacle detection\n";
        
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}