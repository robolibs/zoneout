// Quickstart example demonstrating basic zoneout usage
// Shows: boundary creation → zone creation → raster layer → point containment → path clearance

#include "zoneout/zoneout.hpp"
#include "zoneout/zoneout/constants.hpp"
#include <iostream>

int main() {
    std::cout << "=== Zoneout Quickstart Example ===" << std::endl;

    // Step 1: Create a boundary polygon (100m x 50m rectangular field)
    std::cout << "\n1. Creating boundary polygon..." << std::endl;
    concord::Polygon boundary;
    boundary.addPoint(concord::Point{0.0, 0.0, 0.0});
    boundary.addPoint(concord::Point{100.0, 0.0, 0.0});
    boundary.addPoint(concord::Point{100.0, 50.0, 0.0});
    boundary.addPoint(concord::Point{0.0, 50.0, 0.0});
    std::cout << "   Boundary area: " << boundary.area() << " m²" << std::endl;

    // Step 2: Create a datum (WGS84 coordinates)
    concord::Datum datum{52.0, 5.0, 0.0}; // Lat, Lon, Alt
    std::cout << "\n2. Creating datum at lat=" << datum.lat << ", lon=" << datum.lon << std::endl;

    // Step 3: Create a zone with auto-generated grid (1m resolution)
    std::cout << "\n3. Creating zone with 1m resolution..." << std::endl;
    zoneout::Zone zone("test_field", "agricultural", boundary, datum, 1.0);
    std::cout << "   Zone created: " << zone.name() << " (" << zone.type() << ")" << std::endl;
    std::cout << "   " << zone.raster_info() << std::endl;

    // Step 4: Add properties to the zone
    std::cout << "\n4. Adding zone properties..." << std::endl;
    zone.set_property("crop", "wheat");
    zone.set_property("season", "2024");
    std::cout << "   Crop: " << zone.get_property("crop") << std::endl;
    std::cout << "   Season: " << zone.get_property("season") << std::endl;

    // Step 5: Test point containment
    std::cout << "\n5. Testing point containment..." << std::endl;
    concord::Point inside_point{50.0, 25.0, 0.0};
    concord::Point outside_point{150.0, 25.0, 0.0};

    bool inside = zone.poly().contains(inside_point);
    bool outside = zone.poly().contains(outside_point);

    std::cout << "   Point (50, 25) is " << (inside ? "inside" : "outside") << " the boundary" << std::endl;
    std::cout << "   Point (150, 25) is " << (outside ? "inside" : "outside") << " the boundary" << std::endl;

    // Step 6: Initialize 3D occlusion layer for path planning
    std::cout << "\n6. Initializing 3D occlusion layer..." << std::endl;
    zone.initialize_occlusion_layer(10, 1.0); // 10 layers, 1m height each
    std::cout << "   Occlusion layer initialized" << std::endl;

    // Step 7: Add an obstacle
    std::cout << "\n7. Adding obstacle..." << std::endl;
    concord::Polygon obstacle;
    obstacle.addPoint(concord::Point{40.0, 20.0, 0.0});
    obstacle.addPoint(concord::Point{60.0, 20.0, 0.0});
    obstacle.addPoint(concord::Point{60.0, 30.0, 0.0});
    obstacle.addPoint(concord::Point{40.0, 30.0, 0.0});

    zone.get_occlusion_layer().addPolygonOcclusion(obstacle, 0.0, 3.0, 255);
    std::cout << "   Obstacle added (40-60m x 20-30m, height 0-3m)" << std::endl;

    // Step 8: Test path clearance
    std::cout << "\n8. Testing path clearance..." << std::endl;
    concord::Point start{10.0, 25.0, 1.0};
    concord::Point end_clear{30.0, 25.0, 1.0};
    concord::Point end_blocked{70.0, 25.0, 1.0};

    bool path1_clear =
        zone.is_path_clear(start, end_clear, zoneout::DEFAULT_ROBOT_HEIGHT, zoneout::DEFAULT_PATH_CLEAR_THRESHOLD);
    bool path2_clear =
        zone.is_path_clear(start, end_blocked, zoneout::DEFAULT_ROBOT_HEIGHT, zoneout::DEFAULT_PATH_CLEAR_THRESHOLD);

    std::cout << "   Path from (10,25) to (30,25): " << (path1_clear ? "CLEAR" : "BLOCKED") << std::endl;
    std::cout << "   Path from (10,25) to (70,25): " << (path2_clear ? "CLEAR" : "BLOCKED") << std::endl;

    // Step 9: Save the zone
    std::cout << "\n9. Saving zone..." << std::endl;
    zone.save("quickstart_zone");
    std::cout << "   Zone saved to ./quickstart_zone/" << std::endl;

    // Step 10: Load the zone
    std::cout << "\n10. Loading zone..." << std::endl;
    auto loaded_zone = zoneout::Zone::load("quickstart_zone");
    std::cout << "   Loaded zone: " << loaded_zone.name() << " (" << loaded_zone.type() << ")" << std::endl;
    std::cout << "   Crop: " << loaded_zone.get_property("crop") << std::endl;
    std::cout << "   Has occlusion layer: " << (loaded_zone.has_occlusion_layer() ? "yes" : "no") << std::endl;

    std::cout << "\n=== Quickstart Complete ===" << std::endl;
    return 0;
}
