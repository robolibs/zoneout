#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>
#include <thread>
#include <chrono>

#include "zoneout/zone.hpp"

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

// Helper function to create a simple grid
concord::Grid<float> createSimpleGrid(size_t rows, size_t cols, double resolution, float fill_value = 1.0f) {
    concord::Grid<float> grid(rows, cols, resolution, concord::Datum{}, true, concord::Pose{});
    
    // Fill the grid with values
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            grid.set_value(r, c, fill_value);
        }
    }
    
    return grid;
}

TEST_CASE("Zone basic functionality") {
    SUBCASE("Default construction") {
        Zone zone;
        
        CHECK(!zone.getId().isNull());
        CHECK(zone.getName().empty());
        CHECK(zone.getType() == ZoneType::Other);
        CHECK(!zone.hasBorder());
        CHECK(zone.numLayers() == 0);
        CHECK(!zone.isValid()); // Invalid because no border and no layers
    }
    
    SUBCASE("Named construction") {
        Zone zone("Test Field", ZoneType::Field);
        
        CHECK(!zone.getId().isNull());
        CHECK(zone.getName() == "Test Field");
        CHECK(zone.getType() == ZoneType::Field);
        CHECK(!zone.hasBorder());
        CHECK(zone.numLayers() == 0);
        CHECK(!zone.isValid()); // Still invalid - needs border and layers
    }
    
    SUBCASE("Full construction") {
        auto border = createRectangle(0, 0, 100, 100);
        Zone zone("Test Field", ZoneType::Field, border);
        
        CHECK(!zone.getId().isNull());
        CHECK(zone.getName() == "Test Field");
        CHECK(zone.getType() == ZoneType::Field);
        CHECK(zone.hasBorder());
        CHECK(zone.area() == doctest::Approx(10000.0)); // 100x100
        CHECK(zone.perimeter() == doctest::Approx(400.0)); // 4x100
        CHECK(!zone.isValid()); // Still needs at least one layer
    }
}

TEST_CASE("Zone border management") {
    Zone zone("Test Zone", ZoneType::Field);
    
    SUBCASE("Setting border") {
        auto border = createRectangle(10, 20, 50, 30);
        zone.setBorder(border);
        
        CHECK(zone.hasBorder());
        CHECK(zone.area() == doctest::Approx(1500.0)); // 50x30
        CHECK(zone.perimeter() == doctest::Approx(160.0)); // 2*(50+30)
    }
    
    SUBCASE("Point containment") {
        auto border = createRectangle(0, 0, 100, 100);
        zone.setBorder(border);
        
        CHECK(zone.contains(concord::Point(50, 50, 0)));
        CHECK(zone.contains(concord::Point(10, 90, 0)));
        CHECK(!zone.contains(concord::Point(-10, 50, 0)));
        CHECK(!zone.contains(concord::Point(110, 50, 0)));
        CHECK(!zone.contains(concord::Point(50, 110, 0)));
    }
    
    SUBCASE("Polygon intersection") {
        auto border = createRectangle(0, 0, 100, 100);
        zone.setBorder(border);
        
        // Overlapping polygon
        auto overlapping = createRectangle(50, 50, 100, 100);
        CHECK(zone.intersects(overlapping));
        
        // Non-overlapping polygon
        auto separate = createRectangle(200, 200, 50, 50);
        CHECK(!zone.intersects(separate));
        
        // Contained polygon
        auto contained = createRectangle(10, 10, 20, 20);
        CHECK(zone.intersects(contained));
    }
}

TEST_CASE("Zone layer management") {
    Zone zone("Test Field", ZoneType::Field);
    auto border = createRectangle(0, 0, 100, 100);
    zone.setBorder(border);
    
    SUBCASE("Adding layers") {
        auto grid1 = createSimpleGrid(10, 10, 10.0, 5.0f);
        auto grid2 = createSimpleGrid(20, 20, 5.0, 3.0f);
        
        zone.addLayer("elevation", "height", grid1, 10.0, "meters");
        zone.addLayer("moisture", "moisture", grid2, 5.0, "percentage");
        
        CHECK(zone.numLayers() == 2);
        CHECK(zone.hasLayer("elevation"));
        CHECK(zone.hasLayer("moisture"));
        CHECK(!zone.hasLayer("temperature"));
        
        auto names = zone.getLayerNames();
        CHECK(names.size() == 2);
        CHECK(std::find(names.begin(), names.end(), "elevation") != names.end());
        CHECK(std::find(names.begin(), names.end(), "moisture") != names.end());
    }
    
    SUBCASE("Layer queries") {
        auto grid = createSimpleGrid(10, 10, 10.0, 2.5f);
        zone.addLayer("test_layer", "test_type", grid, 10.0, "units");
        
        const auto* layer = zone.getLayer("test_layer");
        REQUIRE(layer != nullptr);
        CHECK(layer->name == "test_layer");
        CHECK(layer->type == "test_type");
        CHECK(layer->units == "units");
        CHECK(layer->resolution == 10.0);
        
        // Non-existent layer
        CHECK(zone.getLayer("nonexistent") == nullptr);
    }
    
    SUBCASE("Layer by type") {
        auto grid1 = createSimpleGrid(10, 10, 10.0, 1.0f);
        auto grid2 = createSimpleGrid(10, 10, 10.0, 2.0f);
        auto grid3 = createSimpleGrid(10, 10, 10.0, 3.0f);
        
        zone.addLayer("height1", "elevation", grid1, 10.0);
        zone.addLayer("height2", "elevation", grid2, 10.0);
        zone.addLayer("moisture", "water", grid3, 10.0);
        
        auto elevation_layers = zone.getLayersByType("elevation");
        CHECK(elevation_layers.size() == 2);
        
        auto water_layers = zone.getLayersByType("water");
        CHECK(water_layers.size() == 1);
        
        auto missing_layers = zone.getLayersByType("temperature");
        CHECK(missing_layers.size() == 0);
    }
    
    SUBCASE("Remove layer") {
        auto grid = createSimpleGrid(10, 10, 10.0, 1.0f);
        zone.addLayer("temp", "temperature", grid, 10.0);
        
        CHECK(zone.hasLayer("temp"));
        zone.removeLayer("temp");
        CHECK(!zone.hasLayer("temp"));
        CHECK(zone.numLayers() == 0);
    }
}

TEST_CASE("Zone layer grid operations") {
    Zone zone("Test Field", ZoneType::Field);
    auto border = createRectangle(0, 0, 100, 100);
    zone.setBorder(border);
    
    // Create a grid with known values
    auto grid = createSimpleGrid(10, 10, 10.0, 5.0f);
    zone.addLayer("elevation", "height", grid, 10.0, "meters");
    
    SUBCASE("Get value at point") {
        // This is a simplified test - actual implementation depends on grid setup
        CHECK_NOTHROW(zone.getValueAt("elevation", concord::Point(50, 50, 0)));
        CHECK_THROWS_AS(zone.getValueAt("nonexistent", concord::Point(50, 50, 0)), std::invalid_argument);
    }
    
    SUBCASE("Sample grid within polygon") {
        auto sample_area = createRectangle(20, 20, 30, 30);
        CHECK_NOTHROW(zone.sampleGrid("elevation", sample_area));
        CHECK_THROWS_AS(zone.sampleGrid("nonexistent", sample_area), std::invalid_argument);
    }
}

TEST_CASE("Zone subzone management") {
    Zone zone("Test Field", ZoneType::Field);
    auto border = createRectangle(0, 0, 100, 100);
    zone.setBorder(border);
    
    SUBCASE("Adding subzones") {
        auto row1 = createRectangle(10, 10, 80, 10);
        auto row2 = createRectangle(10, 30, 80, 10);
        
        zone.addSubzone("Row 1", row1, SubzoneType::CropRow);
        zone.addSubzone("Row 2", row2, SubzoneType::CropRow);
        
        CHECK(zone.getSubzones().size() == 2);
        
        auto crop_rows = zone.findSubzonesByType(SubzoneType::CropRow);
        CHECK(crop_rows.size() == 2);
        
        auto feeding_areas = zone.findSubzonesByType(SubzoneType::FeedingArea);
        CHECK(feeding_areas.size() == 0);
    }
    
    SUBCASE("Subzone queries") {
        auto subzone_area = createRectangle(20, 20, 30, 30);
        zone.addSubzone("Test Subzone", subzone_area, SubzoneType::WorkArea);
        
        // Find by point
        auto containing = zone.findSubzonesContaining(concord::Point(35, 35, 0));
        CHECK(containing.size() == 1);
        CHECK(containing[0]->name == "Test Subzone");
        
        auto outside = zone.findSubzonesContaining(concord::Point(5, 5, 0));
        CHECK(outside.size() == 0);
        
        // Find by ID
        auto subzone_id = zone.getSubzones()[0].id;
        const auto* found = zone.findSubzone(subzone_id);
        REQUIRE(found != nullptr);
        CHECK(found->name == "Test Subzone");
    }
    
    SUBCASE("Remove subzone") {
        auto subzone_area = createRectangle(20, 20, 30, 30);
        zone.addSubzone("Temp Subzone", subzone_area, SubzoneType::WorkArea);
        
        auto subzone_id = zone.getSubzones()[0].id;
        CHECK(zone.getSubzones().size() == 1);
        
        zone.removeSubzone(subzone_id);
        CHECK(zone.getSubzones().size() == 0);
    }
    
    SUBCASE("Subzone coverage") {
        auto sub1 = createRectangle(10, 10, 20, 20); // 400 area
        auto sub2 = createRectangle(50, 50, 30, 30); // 900 area
        
        zone.addSubzone("Sub1", sub1, SubzoneType::WorkArea);
        zone.addSubzone("Sub2", sub2, SubzoneType::WorkArea);
        
        CHECK(zone.subzoneCoverage() == doctest::Approx(1300.0)); // 400 + 900
        CHECK(zone.subzoneCoverageRatio() == doctest::Approx(0.13)); // 1300/10000
    }
}

TEST_CASE("Zone properties and metadata") {
    Zone zone("Test Zone", ZoneType::Field);
    
    SUBCASE("Basic properties") {
        zone.setProperty("crop_type", "wheat");
        zone.setProperty("season", "spring");
        
        CHECK(zone.getProperty("crop_type") == "wheat");
        CHECK(zone.getProperty("season") == "spring");
        CHECK(zone.getProperty("nonexistent", "default") == "default");
        
        auto properties = zone.getProperties();
        CHECK(properties.size() == 2);
        CHECK(properties.at("crop_type") == "wheat");
    }
    
    SUBCASE("Owner tracking") {
        auto robot_id = generateUUID();
        
        CHECK(!zone.hasOwner());
        
        zone.setOwnerRobot(robot_id);
        CHECK(zone.hasOwner());
        CHECK(zone.getOwnerRobot() == robot_id);
    }
    
    SUBCASE("Time tracking") {
        auto created = zone.getCreatedTime();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        zone.setName("Modified Name");
        
        auto modified = zone.getModifiedTime();
        CHECK(modified > created);
    }
}

TEST_CASE("Zone validation") {
    SUBCASE("Valid zone") {
        auto border = createRectangle(0, 0, 100, 100);
        auto grid = createSimpleGrid(10, 10, 10.0, 1.0f);
        
        Zone zone("Valid Zone", ZoneType::Field, border);
        zone.addLayer("elevation", "height", grid, 10.0);
        
        CHECK(zone.isValid());
    }
    
    SUBCASE("Invalid zones") {
        Zone no_name("", ZoneType::Field);
        CHECK(!no_name.isValid());
        
        Zone no_border("Named", ZoneType::Field);
        CHECK(!no_border.isValid());
        
        auto border = createRectangle(0, 0, 100, 100);
        Zone no_layers("Named", ZoneType::Field, border);
        CHECK(!no_layers.isValid());
    }
}

TEST_CASE("Zone factory methods") {
    auto boundary = createRectangle(0, 0, 100, 50);
    
    SUBCASE("Create field") {
        auto field = Zone::createField("Wheat Field", boundary);
        
        CHECK(field.getType() == ZoneType::Field);
        CHECK(field.getName() == "Wheat Field");
        CHECK(field.getProperty("crop_type") == "unknown");
        CHECK(field.hasBorder());
    }
    
    SUBCASE("Create barn") {
        auto barn = Zone::createBarn("Cow Barn", boundary);
        
        CHECK(barn.getType() == ZoneType::Barn);
        CHECK(barn.getName() == "Cow Barn");
        CHECK(barn.getProperty("animal_type") == "unknown");
        CHECK(barn.getProperty("capacity") == "0");
    }
    
    SUBCASE("Create road") {
        auto road = Zone::createRoad("Main Road", boundary);
        
        CHECK(road.getType() == ZoneType::Road);
        CHECK(road.getName() == "Main Road");
        CHECK(road.getProperty("surface_type") == "unknown");
        CHECK(road.getProperty("max_speed") == "0");
    }
}

TEST_CASE("Zone enum conversions") {
    SUBCASE("Zone type strings") {
        CHECK(zoneTypeToString(ZoneType::Field) == "field");
        CHECK(zoneTypeToString(ZoneType::Barn) == "barn");
        CHECK(zoneTypeToString(ZoneType::SafetyBuffer) == "safety_buffer");
        
        CHECK(zoneTypeFromString("field") == ZoneType::Field);
        CHECK(zoneTypeFromString("barn") == ZoneType::Barn);
        CHECK(zoneTypeFromString("invalid") == ZoneType::Other);
    }
    
    SUBCASE("Subzone type strings") {
        CHECK(subzoneTypeToString(SubzoneType::CropRow) == "crop_row");
        CHECK(subzoneTypeToString(SubzoneType::MilkingStation) == "milking_station");
        
        // Note: No fromString function for subzones yet, but could be added
    }
}

TEST_CASE("Zone buffer creation") {
    auto border = createRectangle(0, 0, 100, 100);
    Zone zone("Original Zone", ZoneType::Field, border);
    zone.setProperty("crop_type", "wheat");
    
    auto buffer = zone.createBufferZone(5.0, "_safety");
    
    CHECK(buffer.getName() == "Original Zone_safety");
    CHECK(buffer.getType() == ZoneType::SafetyBuffer);
    CHECK(buffer.getProperty("crop_type") == "wheat"); // Copied
    CHECK(buffer.getProperty("buffer_distance") == "5");
    CHECK(buffer.getProperty("parent_zone") == zone.getId().toString());
}