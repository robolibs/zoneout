#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <thread>
#include <chrono>

#include "zoneout/zoneout.hpp"

using namespace zoneout;

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
        Zone zone;
        CHECK(zone.getName().empty());
        CHECK(zone.getType() == "other");
        CHECK(!zone.poly_data_.hasFieldBoundary());
        CHECK(zone.getRasterData().gridCount() == 0);
    }

    SUBCASE("Named constructor") {
        Zone zone("Test Zone", "field");
        CHECK(zone.getName() == "Test Zone");
        CHECK(zone.getType() == "field");
        CHECK(!zone.poly_data_.hasFieldBoundary());
        CHECK(zone.getRasterData().gridCount() == 0);
    }

    SUBCASE("Constructor with boundary") {
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary);
        CHECK(zone.getName() == "Test Zone");
        CHECK(zone.getType() == "field");
        CHECK(zone.poly_data_.hasFieldBoundary());
        CHECK(zone.poly_data_.area() == 5000.0); // 100 * 50
    }
}

TEST_CASE("Zone factory methods") {
    auto boundary = createRectangle(0, 0, 100, 50);
    
    SUBCASE("createField") {
        auto field = Zone("Wheat Field", "field", boundary);
        CHECK(field.getName() == "Wheat Field");
        CHECK(field.getType() == "field");
        CHECK(field.poly_data_.hasFieldBoundary());
    }
    
    SUBCASE("createBarn") {
        auto barn = Zone("Main Barn", "barn", boundary);
        CHECK(barn.getName() == "Main Barn");
        CHECK(barn.getType() == "barn");
        CHECK(barn.poly_data_.hasFieldBoundary());
    }
    
    SUBCASE("createGreenhouse") {
        auto greenhouse = Zone("Tomato House", "greenhouse", boundary);
        CHECK(greenhouse.getName() == "Tomato House");
        CHECK(greenhouse.getType() == "greenhouse");
        CHECK(greenhouse.poly_data_.hasFieldBoundary());
    }
}

TEST_CASE("Zone properties") {
    Zone zone("Test Zone", "field");
    
    SUBCASE("Set and get properties") {
        zone.setProperty("crop_type", "wheat");
        zone.setProperty("planted_date", "2024-03-15");
        
        CHECK(zone.getProperty("crop_type") == "wheat");
        CHECK(zone.getProperty("planted_date") == "2024-03-15");
        CHECK(zone.getProperty("non_existent", "default") == "default");
    }
}

TEST_CASE("Zone field elements") {
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary);
    
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
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary);
    
    SUBCASE("Add elevation layer") {
        concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
        
        // Fill with test data
        for (size_t r = 0; r < 10; ++r) {
            for (size_t c = 0; c < 20; ++c) {
                elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + r + c));
            }
        }
        
        zone.getRasterData().addGrid(20, 10, "elevation", "terrain", {{"units", "meters"}});
        
        CHECK(zone.getRasterData().gridCount() == 1);
        
        auto layer_names = zone.getRasterData().getGridNames();
        CHECK(layer_names.size() == 1);
        CHECK(std::find(layer_names.begin(), layer_names.end(), "elevation") != layer_names.end());
    }
}

TEST_CASE("Zone point containment") {
    auto boundary = createRectangle(0, 0, 100, 50);
    Zone zone("Test Zone", "field", boundary);
    
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
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("Test Zone", "field", boundary);
        CHECK(zone.is_valid());
    }
    
    SUBCASE("Invalid zone without boundary") {
        Zone zone("Test Zone", "field");
        CHECK(!zone.is_valid());
    }
    
    SUBCASE("Invalid zone without name") {
        auto boundary = createRectangle(0, 0, 100, 50);
        Zone zone("", "field", boundary);
        CHECK(!zone.is_valid());
    }
}

//TEST_CASE("Zone ownership") {
//    Zone zone("Test Zone", "field");
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
//    Zone zone("Test Zone", "field");
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