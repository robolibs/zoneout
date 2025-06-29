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
        
        Zone zone(WAGENINGEN_DATUM, base_grid);
        CHECK(zone.getName().empty());
        CHECK(zone.getType() == "other");
        CHECK(!zone.poly_data_.hasFieldBoundary());
        CHECK(zone.getRasterData().gridCount() == 1); // Zone now includes the base grid
    }

    SUBCASE("Named constructor") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        Zone zone("Test Zone", "field", WAGENINGEN_DATUM, base_grid);
        CHECK(zone.getName() == "Test Zone");
        CHECK(zone.getType() == "field");
        CHECK(!zone.poly_data_.hasFieldBoundary());
        CHECK(zone.getRasterData().gridCount() == 1); // Zone now includes the base grid
    }

    SUBCASE("Constructor with boundary") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM, base_grid);
        CHECK(zone.getName() == "Test Zone");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly_data_.hasFieldBoundary());
        CHECK(zone.poly_data_.area() == 5000.0); // 100 * 50
    }
}

TEST_CASE("Zone factory methods") {
    auto boundary = createRectangle(0, 0, 100, 50);
    
    SUBCASE("createField") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto field = Zone("Wheat Field", "field", boundary, WAGENINGEN_DATUM, base_grid);
        CHECK(field.getName() == "Wheat Field");
        CHECK(field.getType() == "field");
        CHECK(field.poly_data_.hasFieldBoundary());
    }
    
    SUBCASE("createBarn") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto barn = Zone("Main Barn", "barn", boundary, WAGENINGEN_DATUM, base_grid);
        CHECK(barn.getName() == "Main Barn");
        CHECK(barn.getType() == "barn");
        CHECK(barn.poly_data_.hasFieldBoundary());
    }
    
    SUBCASE("createGreenhouse") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto greenhouse = Zone("Tomato House", "greenhouse", boundary, WAGENINGEN_DATUM, base_grid);
        CHECK(greenhouse.getName() == "Tomato House");
        CHECK(greenhouse.getType() == "greenhouse");
        CHECK(greenhouse.poly_data_.hasFieldBoundary());
    }
}

TEST_CASE("Zone properties") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
    
    Zone zone("Test Zone", "field", WAGENINGEN_DATUM, base_grid);
    
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
    Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM, base_grid);
    
    SUBCASE("Add crop rows") {
        std::vector<concord::Point> row_points;
        row_points.emplace_back(10, 10, 0);
        row_points.emplace_back(90, 10, 0);
        concord::Path crop_row(row_points);
        
        std::unordered_map<std::string, std::string> props;
        props["row_number"] = "1";
        
        zone.poly_data_.addElement(crop_row, "crop_row", props);
        
        auto crop_rows = zone.poly_data_.getElementsByType("crop_row");
        CHECK(crop_rows.size() == 1);
    }
}

TEST_CASE("Zone raster layers") {
    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
    
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM, base_grid);
    
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
    Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM, base_grid);
    
    SUBCASE("Point inside") {
        concord::Point inside_point(50, 25, 0);
        CHECK(zone.poly_data_.contains(inside_point));
    }
    
    SUBCASE("Point outside") {
        concord::Point outside_point(150, 25, 0);
        CHECK(!zone.poly_data_.contains(outside_point));
    }
    
    SUBCASE("Point on boundary") {
        concord::Point boundary_point(0, 25, 0);
        // Note: Polygon.poly_data_.contains() behavior on boundary may vary
        // This test just checks the method works
        zone.poly_data_.contains(boundary_point);
    }
}

TEST_CASE("Zone validation") {
    SUBCASE("Valid zone with boundary") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary, WAGENINGEN_DATUM, base_grid);
        CHECK(zone.is_valid());
    }
    
    SUBCASE("Invalid zone without boundary") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        Zone zone("Test Zone", "field", WAGENINGEN_DATUM, base_grid);
        CHECK(!zone.is_valid());
    }
    
    SUBCASE("Invalid zone without name") {
        // Create simple base grid for Zone constructor
        concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
        concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);
        
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("", "field", boundary, WAGENINGEN_DATUM, base_grid);
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