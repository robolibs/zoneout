#include <doctest/doctest.h>
#include <filesystem>

#include "zoneout/zoneout/layer.hpp"

TEST_CASE("Layer Basic Construction") {
    SUBCASE("Layer basic construction") {
        zoneout::Layer layer;
        CHECK(!layer.getId().isNull());
        CHECK(layer.getName() == "");
        CHECK(layer.getType() == "occlusion");
        CHECK(!layer.isValid()); // No dimensions yet
    }
    
    SUBCASE("Layer construction with name and type") {
        zoneout::Layer layer("Test Occlusion", "obstacle", "navigation");
        CHECK(!layer.getId().isNull());
        CHECK(layer.getName() == "Test Occlusion");
        CHECK(layer.getType() == "obstacle");
        CHECK(layer.getSubtype() == "navigation");
        CHECK(!layer.isValid()); // No dimensions yet
    }
    
    SUBCASE("Layer construction with dimensions") {
        zoneout::Layer layer("3D Map", "occlusion", "robot_nav", 
                            10, 15, 8,  // 10 rows, 15 cols, 8 layers
                            1.0, 0.5);  // 1m cell size, 0.5m layer height
        
        CHECK(!layer.getId().isNull());
        CHECK(layer.getName() == "3D Map");
        CHECK(layer.getType() == "occlusion");
        CHECK(layer.getSubtype() == "robot_nav");
        CHECK(layer.isValid());
        CHECK(layer.rows() == 10);
        CHECK(layer.cols() == 15);
        CHECK(layer.layers() == 8);
        CHECK(layer.inradius() == 1.0);
        CHECK(layer.layer_height() == 0.5);
    }
}

TEST_CASE("Layer Properties and Operations") {
    SUBCASE("Layer with 3D data") {
        zoneout::Layer layer("Test Layer", "occlusion", "default", 
                           5, 6, 4,    // 5×6×4 grid
                           2.0, 1.0,   // 2m cells, 1m layer height
                           concord::Pose{concord::Point{10, 20, 0}, concord::Euler{0, 0, 0}});
        
        CHECK(layer.isValid());
        CHECK(layer.rows() == 5);
        CHECK(layer.cols() == 6);
        CHECK(layer.layers() == 4);
        
        // Test basic data access
        layer.set_value(0, 0, 0, 255); // Set corner to max occlusion
        CHECK(layer.at(0, 0, 0) == 255);
        
        layer(1, 2, 3) = 128; // Set using operator()
        CHECK(layer(1, 2, 3) == 128);
        
        // Test world coordinate access
        concord::Point test_point{10, 20, 0}; // Center of grid
        layer.setOcclusion(test_point, 200);
        CHECK(layer.getOcclusion(test_point) == 200);
    }
    
    SUBCASE("Layer volume operations") {
        zoneout::Layer layer("Volume Test", "occlusion", "test",
                           10, 10, 10,  // 10×10×10 grid
                           1.0, 1.0);   // 1m×1m×1m cells
        
        // Set occlusion for a volume (3D box)
        concord::Point min_point{2.0, 3.0, 1.0};
        concord::Point max_point{4.0, 5.0, 3.0};
        layer.setVolumeOcclusion(min_point, max_point, 255);
        
        // Check that points within volume are occluded
        CHECK(layer.getOcclusion({3.0, 4.0, 2.0}) == 255);
        
        // Check that points outside volume are not occluded
        CHECK(layer.getOcclusion({1.0, 1.0, 1.0}) == 0);
        CHECK(layer.getOcclusion({6.0, 6.0, 6.0}) == 0);
    }
}

TEST_CASE("Layer Robot Navigation") {
    SUBCASE("Path clearance testing") {
        zoneout::Layer layer("Navigation Test", "occlusion", "robot",
                           20, 20, 10,  // 20×20×10 grid
                           0.5, 0.5);   // 0.5m resolution
        
        // Add an obstacle (wall)
        concord::Point wall_min{5.0, 4.0, 0.0};
        concord::Point wall_max{5.5, 6.0, 3.0}; // 3m high wall
        layer.setVolumeOcclusion(wall_min, wall_max, 255);
        
        // Test path that doesn't go through wall  
        concord::Point start{2.0, 2.0, 0.0};
        concord::Point clear_goal{8.0, 2.0, 0.0}; // Around the wall (below it)
        CHECK(layer.isPathClear(start, clear_goal, 2.0)); // 2m robot height
        
        // Test path that goes through wall
        concord::Point blocked_goal{5.25, 5.0, 0.0}; // Through the wall
        CHECK(!layer.isPathClear(start, blocked_goal, 2.0));
    }
    
    SUBCASE("Safe height finding") {
        zoneout::Layer layer("Height Test", "occlusion", "safety",
                           10, 10, 20,   // 10×10×20 grid
                           1.0, 0.5);    // 1m cells, 0.5m layers = 0-10m height
        
        // Add low obstacle (0-2m)
        concord::Point obstacle_min{5.0, 5.0, 0.0};
        concord::Point obstacle_max{5.5, 5.5, 2.0};
        layer.setVolumeOcclusion(obstacle_min, obstacle_max, 255);
        
        // Find safe height at obstacle location
        double safe_height = layer.findSafeHeight(5.25, 5.25, 10.0);
        CHECK(safe_height >= 2.0); // Should be above the obstacle
        
        // Find safe height at clear location
        double clear_height = layer.findSafeHeight(1.0, 1.0, 10.0);
        CHECK(clear_height == 0.0); // Ground level should be safe
    }
}

TEST_CASE("Layer Integration with Polygons") {
    SUBCASE("Polygon occlusion mapping") {
        zoneout::Layer layer("Polygon Test", "occlusion", "integration",
                           20, 20, 5,    // 20×20×5 grid
                           1.0, 1.0);    // 1m resolution
        
        // Create a square polygon obstacle
        concord::Polygon square;
        square.addPoint(concord::Point{8.0, 8.0, 0.0});
        square.addPoint(concord::Point{12.0, 8.0, 0.0});
        square.addPoint(concord::Point{12.0, 12.0, 0.0});
        square.addPoint(concord::Point{8.0, 12.0, 0.0});
        
        // Add polygon as occlusion from 0-2.5m height (so 3.5m is clearly above)
        layer.addPolygonOcclusion(square, 0.0, 2.5, 200);
        
        // Check occlusion inside polygon
        CHECK(layer.getOcclusion({10.0, 10.0, 1.0}) == 200); // Inside, low height
        CHECK(layer.getOcclusion({10.0, 10.0, 2.0}) == 200); // Inside, mid height
        
        // Check no occlusion outside polygon
        CHECK(layer.getOcclusion({5.0, 5.0, 1.0}) == 0);    // Outside
        
        // Check no occlusion outside polygon area (polygon is 8-12, test at 6)  
        CHECK(layer.getOcclusion({6.0, 10.0, 1.0}) == 0);  // Outside polygon bounds
    }
}

TEST_CASE("Layer Grid Integration") {
    SUBCASE("Grid projection to layer") {
        zoneout::Layer layer("Grid Integration", "combined", "test",
                           10, 12, 6,   // 10×12×6 grid
                           1.0, 0.5);   // 1m cells, 0.5m layers
        
        // Create a 2D grid with some data
        concord::Pose grid_pose{concord::Point{0, 0, 0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> source_grid(8, 10, 1.0, true, grid_pose); // Smaller than layer
        
        // Fill grid with test pattern
        for (size_t r = 0; r < source_grid.rows(); ++r) {
            for (size_t c = 0; c < source_grid.cols(); ++c) {
                source_grid.set_value(r, c, static_cast<uint8_t>((r + c) % 256));
            }
        }
        
        // Project to layer at height level 2
        layer.projectGridToLayer(source_grid, 2);
        
        // Verify data was projected correctly
        for (size_t r = 0; r < std::min(source_grid.rows(), layer.rows()); ++r) {
            for (size_t c = 0; c < std::min(source_grid.cols(), layer.cols()); ++c) {
                CHECK(layer(r, c, 2) == source_grid(r, c));
            }
        }
    }
    
    SUBCASE("Grid extraction from layer") {
        zoneout::Layer layer("Extract Test", "source", "test",
                           6, 8, 4,     // 6×8×4 grid
                           2.0, 1.5);   // 2m cells, 1.5m layers
        
        // Fill layer 1 with test data
        for (size_t r = 0; r < layer.rows(); ++r) {
            for (size_t c = 0; c < layer.cols(); ++c) {
                layer(r, c, 1) = static_cast<uint8_t>((r * layer.cols() + c) % 256);
            }
        }
        
        // Extract grid from layer 1
        auto extracted_grid = layer.extractGridFromLayer(1);
        
        // Verify extracted grid matches layer data
        CHECK(extracted_grid.rows() == layer.rows());
        CHECK(extracted_grid.cols() == layer.cols());
        
        for (size_t r = 0; r < layer.rows(); ++r) {
            for (size_t c = 0; c < layer.cols(); ++c) {
                CHECK(extracted_grid(r, c) == layer(r, c, 1));
            }
        }
    }
}

TEST_CASE("Layer Metadata and Properties") {
    SUBCASE("Layer metadata operations") {
        zoneout::Layer layer("Metadata Test", "analysis", "research",
                           15, 20, 12,  // 15×20×12 grid
                           0.8, 0.6);   // 0.8m cells, 0.6m layers
        
        // Test basic properties
        CHECK(layer.getName() == "Metadata Test");
        CHECK(layer.getType() == "analysis");
        CHECK(layer.getSubtype() == "research");
        
        // Test property updates
        layer.setName("Updated Layer");
        layer.setType("updated_type");
        layer.setSubtype("updated_subtype");
        
        CHECK(layer.getName() == "Updated Layer");
        CHECK(layer.getType() == "updated_type");
        CHECK(layer.getSubtype() == "updated_subtype");
        
        // Test metadata export
        auto metadata = layer.getMetadata();
        CHECK(metadata["name"] == "Updated Layer");
        CHECK(metadata["type"] == "updated_type");
        CHECK(metadata["subtype"] == "updated_subtype");
        CHECK(metadata["rows"] == "15");
        CHECK(metadata["cols"] == "20");
        CHECK(metadata["layers"] == "12");
        CHECK(metadata["cell_size"] == "0.800000");
        CHECK(metadata["layer_height"] == "0.600000");
        
        // Test metadata import
        std::unordered_map<std::string, std::string> new_metadata = {
            {"name", "Imported Layer"},
            {"type", "imported"},
            {"subtype", "test"}
        };
        layer.setMetadata(new_metadata);
        
        CHECK(layer.getName() == "Imported Layer");
        CHECK(layer.getType() == "imported");
        CHECK(layer.getSubtype() == "test");
    }
    
    SUBCASE("Layer validation") {
        // Valid layer
        zoneout::Layer valid_layer("Valid", "test", "valid", 5, 5, 5, 1.0, 1.0);
        CHECK(valid_layer.isValid());
        
        // Invalid layer (no dimensions)
        zoneout::Layer invalid_layer;
        CHECK(!invalid_layer.isValid());
        
        // Invalid layer (no name)
        zoneout::Layer no_name_layer("", "test", "valid", 5, 5, 5, 1.0, 1.0);
        CHECK(!no_name_layer.isValid());
    }
}

TEST_CASE("Layer Error Handling") {
    SUBCASE("Out of bounds access") {
        zoneout::Layer layer("Bounds Test", "test", "bounds", 5, 5, 5, 1.0, 1.0);
        
        // Valid access should work
        CHECK_NOTHROW(layer.at(0, 0, 0));
        CHECK_NOTHROW(layer.at(4, 4, 4));
        
        // Out of bounds access should throw
        CHECK_THROWS(layer.at(5, 0, 0));   // Row out of bounds
        CHECK_THROWS(layer.at(0, 5, 0));   // Col out of bounds
        CHECK_THROWS(layer.at(0, 0, 5));   // Layer out of bounds
    }
    
    SUBCASE("Invalid layer operations") {
        zoneout::Layer layer("Invalid Test", "test", "invalid", 3, 3, 3, 1.0, 1.0);
        
        // Invalid layer extraction
        CHECK_THROWS(layer.extractGridFromLayer(5)); // Layer index out of bounds
    }
}

TEST_CASE("Layer File I/O") {
    SUBCASE("Layer save and load round-trip") {
        // Create a layer with test data
        zoneout::Layer original_layer("Test Layer", "occlusion", "file_test",
                                     8, 10, 5,   // 8×10×5 grid
                                     1.5, 0.8);  // 1.5m cells, 0.8m layers
        
        // Fill with test pattern
        for (size_t r = 0; r < original_layer.rows(); ++r) {
            for (size_t c = 0; c < original_layer.cols(); ++c) {
                for (size_t l = 0; l < original_layer.layers(); ++l) {
                    uint8_t value = static_cast<uint8_t>((r + c + l) % 256);
                    original_layer(r, c, l) = value;
                }
            }
        }
        
        // Save to file
        const std::filesystem::path test_file = "/tmp/test_layer_io.tiff";
        original_layer.toFile(test_file);
        
        // Verify file exists
        CHECK(std::filesystem::exists(test_file));
        
        // Load from file
        zoneout::Layer loaded_layer = zoneout::Layer::fromFile(test_file);
        
        // Verify metadata
        CHECK(loaded_layer.getName() == original_layer.getName());
        CHECK(loaded_layer.getType() == original_layer.getType());
        CHECK(loaded_layer.getSubtype() == original_layer.getSubtype());
        CHECK(loaded_layer.rows() == original_layer.rows());
        CHECK(loaded_layer.cols() == original_layer.cols());
        CHECK(loaded_layer.layers() == original_layer.layers());
        CHECK(loaded_layer.inradius() == original_layer.inradius());
        CHECK(loaded_layer.layer_height() == original_layer.layer_height());
        
        // Verify data integrity
        for (size_t r = 0; r < original_layer.rows(); ++r) {
            for (size_t c = 0; c < original_layer.cols(); ++c) {
                for (size_t l = 0; l < original_layer.layers(); ++l) {
                    CHECK(loaded_layer(r, c, l) == original_layer(r, c, l));
                }
            }
        }
        
        // Clean up
        std::filesystem::remove(test_file);
    }
    
    SUBCASE("Layer file error handling") {
        // Test loading non-existent file
        CHECK_THROWS(zoneout::Layer::fromFile("/tmp/non_existent_layer.tiff"));
    }
}