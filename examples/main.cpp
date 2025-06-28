#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>

#include "concord/concord.hpp"
#include "geoson/geoson.hpp"
#include "geotiv/geotiv.hpp"
#include "rerun/recording_stream.hpp"

#include "zoneout/zoneout.hpp"

int main() {
    std::cout << "Zoneout Library Demo - Version " << zoneout::getVersion() << std::endl;

    // Test UUID functionality
    std::cout << "\n=== UUID Testing ===" << std::endl;

    // Generate some UUIDs
    auto uuid1 = zoneout::generateUUID();
    auto uuid2 = zoneout::generateUUID();

    std::cout << "Generated UUID 1: " << uuid1.toString() << std::endl;
    std::cout << "Generated UUID 2: " << uuid2.toString() << std::endl;
    std::cout << "UUIDs are different: " << (uuid1 != uuid2 ? "Yes" : "No") << std::endl;

    // Test string conversion
    std::string uuid_str = uuid1.toString();
    auto uuid1_copy = zoneout::uuidFromString(uuid_str);
    std::cout << "String round-trip works: " << (uuid1 == uuid1_copy ? "Yes" : "No") << std::endl;

    // Test container usage
    std::unordered_map<zoneout::UUID, std::string, zoneout::UUIDHash> zone_names;
    zone_names[uuid1] = "Field A";
    zone_names[uuid2] = "Barn B";

    std::cout << "\n=== UUID in containers ===" << std::endl;
    for (const auto &[uuid, name] : zone_names) {
        std::cout << "Zone " << uuid.toString() << " -> " << name << std::endl;
    }

    // Test null UUID
    auto null_uuid = zoneout::UUID::null();
    std::cout << "\nNull UUID: " << null_uuid.toString() << std::endl;
    std::cout << "Is null: " << (null_uuid.isNull() ? "Yes" : "No") << std::endl;

    std::cout << "\n=== Time Utilities Testing ===" << std::endl;

    // Test time utilities
    auto current_time = zoneout::time_utils::now();
    std::cout << "Current time (ISO 8601): " << zoneout::time_utils::toISO8601(current_time) << std::endl;

    // Test duration helpers
    auto duration1 = zoneout::time_utils::hours(2.5);
    auto duration2 = zoneout::time_utils::minutes(30);
    auto duration3 = zoneout::time_utils::seconds(45.5);

    std::cout << "2.5 hours: " << zoneout::time_utils::durationToString(duration1) << std::endl;
    std::cout << "30 minutes: " << zoneout::time_utils::durationToString(duration2) << std::endl;
    std::cout << "45.5 seconds: " << zoneout::time_utils::durationToString(duration3) << std::endl;

    // Test future time calculation
    auto future_time = zoneout::time_utils::add(current_time, zoneout::time_utils::hours(1));
    std::cout << "One hour from now: " << zoneout::time_utils::toISO8601(future_time) << std::endl;
    std::cout << "Time until then: "
              << zoneout::time_utils::durationToString(zoneout::time_utils::timeUntil(future_time)) << std::endl;

    // Test Lamport clock for distributed coordination
    std::cout << "\n=== Lamport Clock Testing ===" << std::endl;
    zoneout::LamportClock robot_clock;

    auto t1 = robot_clock.tick();
    auto t2 = robot_clock.tick();
    std::cout << "Robot clock: " << t1 << " -> " << t2 << std::endl;

    // Simulate receiving message from another robot with timestamp 5
    auto t3 = robot_clock.update(5);
    std::cout << "After receiving remote timestamp 5: " << t3 << std::endl;
    std::cout << "Current logical time: " << robot_clock.time() << std::endl;

    std::cout << "\n=== Zone Core Testing ===" << std::endl;

    // Create a field zone
    std::vector<concord::Point> field_points;
    field_points.emplace_back(0.0, 0.0, 0.0);
    field_points.emplace_back(100.0, 0.0, 0.0);
    field_points.emplace_back(100.0, 50.0, 0.0);
    field_points.emplace_back(0.0, 50.0, 0.0);

    concord::Polygon field_boundary(field_points);
    auto field = zoneout::Zone::createField("Wheat Field Alpha", field_boundary);

    std::cout << "Created field zone: " << field.getName() << std::endl;
    std::cout << "Zone ID: " << field.getId().toString() << std::endl;
    std::cout << "Zone type: " << zoneout::zoneTypeToString(field.getType()) << std::endl;
    std::cout << "Field area: " << field.area() << " m²" << std::endl;
    std::cout << "Field perimeter: " << field.perimeter() << " m" << std::endl;

    // Add some properties
    field.setProperty("crop_type", "wheat");
    field.setProperty("planting_date", "2024-03-15");
    field.setProperty("expected_harvest", "2024-08-15");

    std::cout << "Crop type: " << field.getProperty("crop_type") << std::endl;

    // Create elevation layer
    concord::Grid<float> elevation_grid(10, 20, 5.0, concord::Datum{}, true, concord::Pose{});
    for (size_t r = 0; r < 10; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            elevation_grid.set_value(r, c, 100.0f + (r * c * 0.1f)); // Gradient elevation
        }
    }
    field.addLayer("elevation", "height", elevation_grid, 5.0, "meters");

    std::cout << "Added elevation layer with " << field.numLayers() << " total layers" << std::endl;

    // Add crop rows as subzones
    for (int i = 0; i < 5; ++i) {
        std::vector<concord::Point> row_points;
        double y_start = 5.0 + i * 8.0;
        row_points.emplace_back(5.0, y_start, 0.0);
        row_points.emplace_back(95.0, y_start, 0.0);
        row_points.emplace_back(95.0, y_start + 6.0, 0.0);
        row_points.emplace_back(5.0, y_start + 6.0, 0.0);

        concord::Polygon row_boundary(row_points);
        field.addSubzone("Row " + std::to_string(i + 1), row_boundary, zoneout::SubzoneType::CropRow);
    }

    std::cout << "Added " << field.getSubzones().size() << " crop rows" << std::endl;
    std::cout << "Subzone coverage: " << field.subzoneCoverage() << " m² (" << (field.subzoneCoverageRatio() * 100)
              << "%)" << std::endl;

    // Test point queries
    concord::Point test_point(50.0, 25.0, 0.0);
    std::cout << "\nTesting point (50, 25):" << std::endl;
    std::cout << "In field: " << (field.contains(test_point) ? "Yes" : "No") << std::endl;

    auto containing_subzones = field.findSubzonesContaining(test_point);
    std::cout << "In " << containing_subzones.size() << " subzones";
    if (!containing_subzones.empty()) {
        std::cout << " (e.g., " << containing_subzones[0]->name << ")";
    }
    std::cout << std::endl;

    // Test elevation sampling
    if (field.hasLayer("elevation")) {
        try {
            float elevation = field.getValueAt("elevation", test_point);
            std::cout << "Elevation at test point: " << elevation << " meters" << std::endl;
        } catch (const std::exception &e) {
            std::cout << "Could not sample elevation: " << e.what() << std::endl;
        }
    }

    // Create a barn zone
    std::vector<concord::Point> barn_points;
    barn_points.emplace_back(120.0, 10.0, 0.0);
    barn_points.emplace_back(140.0, 10.0, 0.0);
    barn_points.emplace_back(140.0, 30.0, 0.0);
    barn_points.emplace_back(120.0, 30.0, 0.0);

    concord::Polygon barn_boundary(barn_points);
    auto barn = zoneout::Zone::createBarn("Main Barn", barn_boundary);

    // Add barn sections
    std::vector<concord::Point> milking_points;
    milking_points.emplace_back(122.0, 12.0, 0.0);
    milking_points.emplace_back(138.0, 12.0, 0.0);
    milking_points.emplace_back(138.0, 18.0, 0.0);
    milking_points.emplace_back(122.0, 18.0, 0.0);

    concord::Polygon milking_boundary(milking_points);
    barn.addSubzone("Milking Area", milking_boundary, zoneout::SubzoneType::MilkingStation);

    std::cout << "\nCreated barn: " << barn.getName() << std::endl;
    std::cout << "Barn area: " << barn.area() << " m²" << std::endl;
    std::cout << "Barn subzones: " << barn.getSubzones().size() << std::endl;

    // Validation
    std::cout << "\nZone validation:" << std::endl;
    std::cout << "Field valid: " << (field.isValid() ? "Yes" : "No") << std::endl;
    std::cout << "Barn valid: " << (barn.isValid() ? "No (needs layers)" : "Yes") << std::endl;

    std::cout << "\n=== Demo completed successfully! ===" << std::endl;

    return 0;
}
