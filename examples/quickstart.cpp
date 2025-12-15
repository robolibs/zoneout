#include "zoneout/zoneout.hpp"
#include "zoneout/zoneout/constants.hpp"
#include <spdlog/spdlog.h>

int main() {
    std::cout << "=== Zoneout Quickstart Example ===" << std::endl;

    // Step 1: Create a boundary polygon (100m x 50m rectangular field)
    spdlog::info("Creating boundary polygon...");
    concord::Polygon boundary;
    boundary.addPoint(concord::Point{0.0, 0.0, 0.0});
    boundary.addPoint(concord::Point{100.0, 0.0, 0.0});
    boundary.addPoint(concord::Point{100.0, 50.0, 0.0});
    boundary.addPoint(concord::Point{0.0, 50.0, 0.0});
    spdlog::info("   Boundary created: {}", boundary.getPoints().size());

    // Step 2: Create a datum (WGS84 coordinates)
    concord::Datum datum{52.0, 5.0, 0.0}; // Lat, Lon, Alt
    spdlog::info("Creating datum at lat={}, lon={}", datum.lat, datum.lon);

    // Step 3: Create a zone with auto-generated grid (1m resolution)
    spdlog::info("Creating zone with 1m resolution...");
    zoneout::Zone zone("test_field", "agricultural", boundary, datum, 1.0);
    spdlog::info("   Zone created: {} ({})", zone.name(), zone.type());
    spdlog::info("   {}", zone.raster_info());

    // Step 4: Add properties to the zone
    spdlog::info("Adding properties to zone...");
    zone.set_property("crop", "wheat");
    zone.set_property("season", "2024");
    spdlog::info("   Crop: {}", zone.get_property("crop"));
    spdlog::info("   Season: {}", zone.get_property("season"));

    // Step 5: Test point containment
    spdlog::info("Testing point containment...");
    concord::Point inside_point{50.0, 25.0, 0.0};
    concord::Point outside_point{150.0, 25.0, 0.0};

    bool inside = zone.poly().contains(inside_point);
    bool outside = zone.poly().contains(outside_point);

    spdlog::info("   Point (50, 25) is {} inside the boundary", inside ? "inside" : "outside");
    spdlog::info("   Point (150, 25) is {} inside the boundary", outside ? "inside" : "outside");

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
