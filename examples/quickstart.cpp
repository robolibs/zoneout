#include "zoneout/zoneout.hpp"
#include "zoneout/zoneout/constants.hpp"
#include <iostream>

namespace dp = datapod;

int main() {
    std::cout << "=== Zoneout Quickstart Example ===" << std::endl;

    // Step 1: Create a boundary polygon (100m x 50m rectangular field)
    std::cout << "Creating boundary polygon..." << std::endl;
    dp::Polygon boundary;
    boundary.vertices.push_back(dp::Point{0.0, 0.0, 0.0});
    boundary.vertices.push_back(dp::Point{100.0, 0.0, 0.0});
    boundary.vertices.push_back(dp::Point{100.0, 50.0, 0.0});
    boundary.vertices.push_back(dp::Point{0.0, 50.0, 0.0});
    std::cout << "   Boundary created: " << boundary.vertices.size() << std::endl;

    // Step 2: Create a datum (WGS84 coordinates)
    dp::Geo datum{52.0, 5.0, 0.0}; // Lat, Lon, Alt
    std::cout << "Creating datum at lat=" << datum.lat << ", lon=" << datum.lon << std::endl;

    // Step 3: Create a zone with auto-generated grid (1m resolution)
    std::cout << "Creating zone with 1m resolution..." << std::endl;
    zoneout::Zone zone("test_field", "agricultural", boundary, datum, 1.0);
    std::cout << "   Zone created: " << zone.name() << " (" << zone.type() << ")" << std::endl;
    std::cout << "   " << zone.raster_info() << std::endl;

    // Step 4: Add properties to the zone
    std::cout << "Adding properties to zone..." << std::endl;
    zone.set_property("crop", "wheat");
    zone.set_property("season", "2024");
    std::cout << "   Crop: " << zone.get_property("crop") << std::endl;
    std::cout << "   Season: " << zone.get_property("season") << std::endl;

    // Step 5: Test point containment
    std::cout << "Testing point containment..." << std::endl;
    dp::Point inside_point{50.0, 25.0, 0.0};
    dp::Point outside_point{150.0, 25.0, 0.0};

    bool inside = zone.poly().contains(inside_point);
    bool outside = zone.poly().contains(outside_point);

    std::cout << "   Point (50, 25) is " << (inside ? "inside" : "outside") << " the boundary" << std::endl;
    std::cout << "   Point (150, 25) is " << (outside ? "inside" : "outside") << " the boundary" << std::endl;

    // Step 6: Save the zone
    std::cout << "Saving zone..." << std::endl;
    zone.save("quickstart_zone");
    std::cout << "   Zone saved to ./quickstart_zone/" << std::endl;

    // Step 10: Load the zone
    std::cout << "Loading zone..." << std::endl;
    auto loaded_zone = zoneout::Zone::load("quickstart_zone");
    std::cout << "   Loaded zone: " << loaded_zone.name() << " (" << loaded_zone.type() << ")" << std::endl;
    std::cout << "   Crop: " << loaded_zone.get_property("crop") << std::endl;

    std::cout << "=== Quickstart Complete ===" << std::endl;
    return 0;
}
