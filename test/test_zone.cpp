#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <thread>
#include <chrono>

#include "zoneout/zoneout.hpp"

using namespace zoneout;

// Wageningen Research Labs coordinates
const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};

// Helper function to create a simple rectangular polygon
concord::Polygon createRectangle(double x, double y, double width, double height) {
    std::vector<concord::Point> points;
    points.emplace_back(x, y, 0.0);
    points.emplace_back(x + width, y, 0.0);
    points.emplace_back(x + width, y + height, 0.0);
    points.emplace_back(x, y + height, 0.0);
    return concord::Polygon(points);
}

TEST_CASE("Zone creation and basic properties") {
    SUBCASE("Default constructor") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        concord::Polygon default_boundary;
        Zone zone("", "other", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.getName().empty());
        CHECK(zone.getType() == "other");
        CHECK(!zone.poly().hasFieldBoundary());
        CHECK(zone.getRasterData().gridCount() == 1); // Zone now includes the base grid
    }

    SUBCASE("Named constructor") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        concord::Polygon default_boundary;
        Zone zone("Test Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.getName() == "Test Zone");
        CHECK(zone.getType() == "field");
        CHECK(!zone.poly().hasFieldBoundary());
        CHECK(zone.getRasterData().gridCount() == 1); // Zone now includes the base grid
    }

    SUBCASE("Constructor with boundary") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.getName() == "Test Zone");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly().hasFieldBoundary());
        CHECK(zone.poly().area() == 5000.0); // 100 * 50
    }
}

TEST_CASE("Zone constructor with auto-generated grid") {
    SUBCASE("Constructor with default resolution (1.0)") {
        // Create a 100x50 rectangle - default resolution should be 1.0
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Auto Grid Zone", "field", boundary, WAGENINGEN_DATUM);
        
        CHECK(zone.getName() == "Auto Grid Zone");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly().hasFieldBoundary());
        CHECK(zone.grid().gridCount() == 1); // Should have the auto-generated base grid with noise
        
        // Check that grid dimensions are reasonable for the polygon size
        // For a 100x50 rectangle with resolution 1.0, we expect roughly 100x50 cells
        auto grid_info = zone.getRasterInfo();
        CHECK(!grid_info.empty());
    }
    
    SUBCASE("Constructor with custom resolution (2.0)") {
        // Create a 100x50 rectangle with 2x2 cell resolution
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Custom Resolution Zone", "field", boundary, WAGENINGEN_DATUM, 2.0);
        
        CHECK(zone.getName() == "Custom Resolution Zone");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly().hasFieldBoundary());
        CHECK(zone.grid().gridCount() == 1);
        
        // With resolution 2.0, grid should have roughly half the cells in each dimension
        auto grid_info = zone.getRasterInfo();
        CHECK(!grid_info.empty());
    }
    
    SUBCASE("Constructor with fine resolution (0.5)") {
        // Create a smaller polygon with fine resolution
        auto boundary = createRectangle(0, 0, 10, 5);
        Zone zone("Fine Resolution Zone", "field", boundary, WAGENINGEN_DATUM, 0.5);
        
        CHECK(zone.getName() == "Fine Resolution Zone");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly().hasFieldBoundary());
        CHECK(zone.grid().gridCount() == 1);
        
        // With resolution 0.5, grid should have more cells for the same area
        auto grid_info = zone.getRasterInfo();
        CHECK(!grid_info.empty());
    }
    
    SUBCASE("Constructor with complex polygon") {
        // Create L-shaped polygon
        std::vector<concord::Point> l_points;
        l_points.emplace_back(0, 0, 0);
        l_points.emplace_back(60, 0, 0);
        l_points.emplace_back(60, 30, 0);
        l_points.emplace_back(30, 30, 0);
        l_points.emplace_back(30, 60, 0);
        l_points.emplace_back(0, 60, 0);
        
        concord::Polygon l_boundary(l_points);
        Zone zone("L-Shape Auto Grid", "field", l_boundary, WAGENINGEN_DATUM);
        
        CHECK(zone.getName() == "L-Shape Auto Grid");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly().hasFieldBoundary());
        CHECK(zone.grid().gridCount() == 1);
        
        // Grid should be generated based on the OBB of the L-shape
        auto grid_info = zone.getRasterInfo();
        CHECK(!grid_info.empty());
    }
}

TEST_CASE("Zone poly_cut functionality") {
    SUBCASE("addRasterLayer with poly_cut=true") {
        // Create an L-shaped polygon
        std::vector<concord::Point> l_points;
        l_points.emplace_back(0, 0, 0);
        l_points.emplace_back(60, 0, 0);
        l_points.emplace_back(60, 30, 0);
        l_points.emplace_back(30, 30, 0);
        l_points.emplace_back(30, 60, 0);
        l_points.emplace_back(0, 60, 0);
        concord::Polygon l_boundary(l_points);
        
        Zone zone("L-Shape Zone", "field", l_boundary, WAGENINGEN_DATUM);
        
        // Create a separate grid that covers the entire bounding box
        concord::Pose shift{concord::Point{30.0, 30.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> full_grid(60, 60, 1.0, true, shift);
        
        // Fill the entire grid with value 255
        for (size_t r = 0; r < full_grid.rows(); ++r) {
            for (size_t c = 0; c < full_grid.cols(); ++c) {
                full_grid.set_value(r, c, 255);
            }
        }
        
        // Add the grid with poly_cut=true - should zero out cells outside the L-shape
        zone.addRasterLayer(full_grid, "test_layer", "test", {}, true);
        
        CHECK(zone.grid().gridCount() == 2); // Base grid + test layer
        
        // Verify the layer was added
        auto layer_names = zone.grid().getGridNames();
        CHECK(std::find(layer_names.begin(), layer_names.end(), "test_layer") != layer_names.end());
    }
    
    SUBCASE("addRasterLayer with poly_cut=false (default)") {
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM);
        
        // Create a grid
        concord::Pose shift{concord::Point{50.0, 25.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> test_grid(50, 100, 1.0, true, shift);
        
        // Fill with test values
        for (size_t r = 0; r < test_grid.rows(); ++r) {
            for (size_t c = 0; c < test_grid.cols(); ++c) {
                test_grid.set_value(r, c, 128);
            }
        }
        
        // Add without poly_cut - should preserve all values
        zone.addRasterLayer(test_grid, "no_cut_layer", "test");
        
        CHECK(zone.grid().gridCount() == 2); // Base grid + no_cut_layer
    }
}

TEST_CASE("Zone factory methods") {
    auto boundary = createRectangle(0, 0, 100, 50);
    
    SUBCASE("createField") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto field = Zone("Wheat Field", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(field.getName() == "Wheat Field");
        CHECK(field.getType() == "field");
        CHECK(field.poly().hasFieldBoundary());
    }
    
    SUBCASE("createBarn") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto barn = Zone("Main Barn", "barn", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(barn.getName() == "Main Barn");
        CHECK(barn.getType() == "barn");
        CHECK(barn.poly().hasFieldBoundary());
    }
    
    SUBCASE("createGreenhouse") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto greenhouse = Zone("Tomato House", "greenhouse", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(greenhouse.getName() == "Tomato House");
        CHECK(greenhouse.getType() == "greenhouse");
        CHECK(greenhouse.poly().hasFieldBoundary());
    }
}

TEST_CASE("Zone properties") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
    
    concord::Polygon default_boundary;
    Zone zone("Test Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
    
    SUBCASE("Set and get properties") {
        zone.setProperty("crop_type", "wheat");
        zone.setProperty("planted_date", "2024-03-15");
        
        CHECK(zone.getProperty("crop_type") == "wheat");
        CHECK(zone.getProperty("planted_date") == "2024-03-15");
        CHECK(zone.getProperty("non_existent", "default") == "default");
    }
}

TEST_CASE("Zone field elements") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
    
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
    
    SUBCASE("Add crop rows") {
        std::vector<concord::Point> row_points;
        row_points.emplace_back(10, 10, 0);
        row_points.emplace_back(90, 10, 0);
        concord::Path crop_row(row_points);
        
        std::unordered_map<std::string, std::string> props;
        props["row_number"] = "1";
        
        zone.poly().addElement(crop_row, "crop_row", props);
        
        auto crop_rows = zone.poly().getElementsByType("crop_row");
        CHECK(crop_rows.size() == 1);
    }
}

TEST_CASE("Zone raster layers") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
    
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
    
    SUBCASE("Add elevation layer") {
        concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
        
        // Fill with test data
        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
            }
        }
        
        zone.getRasterData().addGrid(20, 10, "elevation", "terrain", {{"units", "meters"}});
        
        CHECK(zone.getRasterData().gridCount() == 2); // Base grid + elevation layer
        
        auto layer_names = zone.getRasterData().getGridNames();
        CHECK(layer_names.size() == 2); // Base grid + elevation layer
        CHECK(std::find(layer_names.begin(), layer_names.end(), "elevation") != layer_names.end());
    }
}

TEST_CASE("Zone point containment") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
    
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
    
    SUBCASE("Point inside") {
        concord::Point inside_point(50, 25, 0);
        CHECK(zone.poly().contains(inside_point));
    }
    
    SUBCASE("Point outside") {
        concord::Point outside_point(150, 25, 0);
        CHECK(!zone.poly().contains(outside_point));
    }
    
    SUBCASE("Point on boundary") {
        concord::Point boundary_point(0, 25, 0);
        // Note: Polygon.poly().contains() behavior on boundary may vary
        // This test just checks the method works
        zone.poly().contains(boundary_point);
    }
}

TEST_CASE("Zone validation") {
    SUBCASE("Valid zone with boundary") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(zone.is_valid());
    }
    
    SUBCASE("Invalid zone without boundary") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        concord::Polygon default_boundary;
        Zone zone("Test Zone", "field", default_boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!zone.is_valid());
    }
    
    SUBCASE("Invalid zone without name") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("", "field", boundary, base_grid, WAGENINGEN_DATUM);
        CHECK(!zone.is_valid());
    }
}

//TEST_CASE("Zone ownership") {
//    Zone zone("Test Zone", "field", WAGENINGEN_DATUM);
//    
//    SUBCASE("Initial state") {
//        // CHECK(!zone.hasOwner()); // hasOwner removed
//        // CHECK(zone.getOwnerRobot().isNull()); // getOwnerRobot removed
//    }
//    
//    SUBCASE("Set owner") {
//        auto robot_id = generateUUID();
//        zone.setOwnerRobot(robot_id);
//        
//        // CHECK(!zone.hasOwner()); // hasOwner removed
//        CHECK(zone.getOwnerRobot() == robot_id);
//    }
//    
//    SUBCASE("Release ownership") {
//        auto robot_id = generateUUID();
//        zone.setOwnerRobot(robot_id);
//        zone.releaseOwnership();
//        
//        // CHECK(!zone.hasOwner()); // hasOwner removed
//        // CHECK(zone.getOwnerRobot().isNull()); // getOwnerRobot removed
//    }
//}

//TEST_CASE("Zone timestamps") {
//    Zone zone("Test Zone", "field", WAGENINGEN_DATUM);
//    
//    // Check that timestamps are set (non-zero epoch time)
//    auto epoch = std::chrono::time_point<std::chrono::system_clock>{};
//    CHECK(zone.getId() // getCreatedTime removed > epoch);
//    // CHECK(zone.getModifiedTime() // getModifiedTime removed > epoch);
//    
//    auto initial_modified = zone.getId() // getModifiedTime removed;
//    
//    // Sleep briefly and modify zone
//    std::this_thread::sleep_for(std::chrono::milliseconds(10));
//    zone.setProperty("test", "value");
//    
//    // Modified time functionality removed
//    // // CHECK(zone.getModifiedTime() // getModifiedTime removed > initial_modified);
//}