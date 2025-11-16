#include <iostream>
#include <zoneout/zoneout.hpp>

#ifdef HAS_RERUN
#include <rerun/recording_stream.hpp>
#endif

int main() {
    std::cout << "Zoneout Agricultural Visualization Example" << std::endl;

#ifdef HAS_RERUN
    std::cout << "Visualization enabled with Rerun SDK" << std::endl;

    // Initialize Rerun connection
    auto rec = std::make_shared<rerun::RecordingStream>("zoneout_agricultural", "space");
    auto result = rec->connect_grpc("rerun+http://0.0.0.0:9876/proxy");
    if (result.is_err()) {
        std::cout << "Failed to connect to Rerun viewer" << std::endl;
        return 1;
    }

    // Set up realistic agricultural coordinates
    // Use Wageningen Research Labs (Netherlands) as reference - real agricultural research location
    concord::Datum datum{51.98776171041831, 5.662378206146002, 0.0}; // Wageningen Research Labs

    // Create simple base grid for Zone constructor
    concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
    concord::Grid<uint8_t> base_grid(10, 10, 1.0, true, shift);

    // Create a realistic agricultural zone (wheat field)
    concord::Polygon default_boundary;
    zoneout::Zone wheat_field("Wheat_Field_North", "field", default_boundary, base_grid, datum);

    // Create field boundary in local ENU coordinates (meters from datum)
    concord::Polygon boundary;
    boundary.addPoint(concord::Point(0.0, 0.0, 0.0));     // SW corner
    boundary.addPoint(concord::Point(300.0, 0.0, 0.0));   // SE corner
    boundary.addPoint(concord::Point(300.0, 200.0, 0.0)); // NE corner
    boundary.addPoint(concord::Point(0.0, 200.0, 0.0));   // NW corner

    wheat_field.poly().setFieldBoundary(boundary);
    wheat_field.setProperty("crop_type", "wheat");
    wheat_field.setProperty("planting_date", "2024-10-15");
    wheat_field.setProperty("area_hectares", "6.0");

    std::cout << "Created zone: " << wheat_field.getName() << " (" << wheat_field.getType()
              << "): " << wheat_field.poly().area() << " m²" << std::endl;

    // Visualize the zone
    std::cout << "\nSending visualization data to Rerun..." << std::endl;
    zoneout::visualize::visualize_zone(wheat_field, rec, datum, wheat_field.getName(), 0);

    std::cout << "\n=== Visualization Ready ===" << std::endl;
    std::cout << "Open your browser to: http://localhost:9876" << std::endl;
    std::cout << "Or run: rerun &" << std::endl;
    std::cout << "\nVisualization features:" << std::endl;
    std::cout << "• Local ENU coordinates: /" << wheat_field.getName() << "/enu (meters from datum)" << std::endl;
    std::cout << "• GPS map coordinates: /" << wheat_field.getName() << "/wgs (lat/lon on world map)" << std::endl;
    std::cout << "\nMap view shows real GPS coordinates around Wageningen, Netherlands" << std::endl;
    std::cout << "\nZone area: " << wheat_field.poly().area() / 10000.0 << " hectares" << std::endl;
    std::cout << "Datum reference: " << datum.lat << "°N, " << datum.lon << "°E" << std::endl;

    // Keep the program running to maintain the visualization
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();

#else
    std::cout << "Visualization disabled - build with ZONEOUT_BUILD_EXAMPLES=ON to enable" << std::endl;
    std::cout << "This example demonstrates realistic agricultural zone coordinates and proper scale." << std::endl;
    std::cout << "Zone is 6.0 hectares - typical field size." << std::endl;
#endif

    return 0;
}
