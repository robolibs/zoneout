#include <iomanip>
#include <iostream>
#include <thread>
#include <unordered_map>

#include "concord/concord.hpp"
#include "geoson/geoson.hpp"
#include "geotiv/geotiv.hpp"
#include "rerun/recording_stream.hpp"

#include "zoneout/zoneout.hpp"

// Helper function to create a rectangular polygon
concord::Polygon createRectangle(double x, double y, double width, double height) {
    std::vector<concord::Point> points;
    points.emplace_back(x, y, 0.0);
    points.emplace_back(x + width, y, 0.0);
    points.emplace_back(x + width, y + height, 0.0);
    points.emplace_back(x, y + height, 0.0);
    return concord::Polygon(points);
}

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
    std::cout << "Zone type: " << field.getType() << std::endl;
    std::cout << "Field area: " << field.area() << " m²" << std::endl;
    std::cout << "Field perimeter: " << field.perimeter() << " m" << std::endl;

    // Add some properties
    field.setProperty("crop_type", "wheat");
    field.setProperty("planting_date", "2024-03-15");
    field.setProperty("expected_harvest", "2024-08-15");

    std::cout << "Crop type: " << field.getProperty("crop_type") << std::endl;

    // Create elevation layer
    concord::Grid<uint8_t> elevation_grid(10, 20, 5.0, true, concord::Pose{});
    for (size_t r = 0; r < 10; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            elevation_grid.set_value(r, c, static_cast<uint8_t>(100 + (r * c / 10))); // Gradient elevation
        }
    }
    field.add_layer("elevation", "terrain", elevation_grid, {{"units", "meters"}});

    std::cout << "Added elevation layer with " << field.num_layers() << " total layers" << std::endl;

    // Add crop rows as field elements
    for (int i = 0; i < 5; ++i) {
        std::vector<concord::Point> row_points;
        double y_start = 5.0 + i * 8.0;
        row_points.emplace_back(5.0, y_start, 0.0);
        row_points.emplace_back(95.0, y_start, 0.0);

        concord::Path crop_row(row_points);
        std::unordered_map<std::string, std::string> row_props;
        row_props["row_number"] = std::to_string(i + 1);
        row_props["crop_type"] = "wheat";
        field.add_element(crop_row, "crop_row", row_props);
    }

    std::cout << "Added " << field.get_elements("crop_row").size() << " crop rows" << std::endl;

    // Test point queries
    concord::Point test_point(50.0, 25.0, 0.0);
    std::cout << "\nTesting point (50, 25):" << std::endl;
    std::cout << "In field: " << (field.contains(test_point) ? "Yes" : "No") << std::endl;

    // Test elevation sampling
    if (field.has_layer("elevation")) {
        auto elevation = field.sample_at("elevation", test_point);
        if (elevation) {
            std::cout << "Elevation at test point: " << static_cast<int>(*elevation) << " meters" << std::endl;
        } else {
            std::cout << "Could not sample elevation at test point" << std::endl;
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

    std::cout << "\nCreated barn: " << barn.getName() << std::endl;
    std::cout << "Barn area: " << barn.area() << " m²" << std::endl;

    // Validation
    std::cout << "\nZone validation:" << std::endl;
    std::cout << "Field valid: " << (field.is_valid() ? "Yes" : "No") << std::endl;
    std::cout << "Barn valid: " << (barn.is_valid() ? "Yes" : "No") << std::endl;

    // Test file I/O with different element types
    std::cout << "\n=== File I/O Testing ===" << std::endl;
    
    // Add different types of elements to the field
    auto parking_area = createRectangle(110, 60, 20, 15);
    field.add_element(parking_area, "parking_space", {
        {"name", "main_parking"},
        {"capacity", "5_vehicles"}, 
        {"surface", "gravel"}
    });
    
    auto storage_zone = createRectangle(80, 70, 25, 20);
    field.add_element(storage_zone, "storage_area", {
        {"name", "equipment_storage"},
        {"max_weight", "500kg_per_m2"},
        {"weather_protection", "covered"}
    });
    
    std::vector<concord::Point> access_path = {{5, 50, 0}, {95, 50, 0}};
    field.add_element(concord::Path(access_path), "access_route", {
        {"name", "main_access"},
        {"width", "4m"},
        {"surface", "dirt_road"}
    });
    
    field.add_element(concord::Point(60, 40, 0), "equipment_point", {
        {"name", "water_station"},
        {"type", "irrigation_hub"},
        {"flow_rate", "100L_per_min"}
    });
    
    std::cout << "Added " << field.get_elements().size() << " elements to field:" << std::endl;
    for (const auto& element : field.get_elements()) {
        auto name_it = element.properties.find("name");
        auto type_it = element.properties.find("type");
        if (name_it != element.properties.end() && type_it != element.properties.end()) {
            std::cout << "- " << name_it->second << " (" << type_it->second << ")" << std::endl;
        }
    }
    
    // Save to files
    std::string vector_path = "/tmp/test_field.geojson";
    std::string raster_path = "/tmp/test_field.tiff";
    
    try {
        field.toFiles(vector_path, raster_path);
        std::cout << "\nSaved field to: " << vector_path << std::endl;
        
        // Load it back
        auto loaded_field = zoneout::Zone::fromFiles(vector_path, raster_path);
        std::cout << "Loaded field: " << loaded_field.getName() << std::endl;
        std::cout << "Loaded elements: " << loaded_field.get_elements().size() << std::endl;
        std::cout << "Loaded layers: " << loaded_field.num_layers() << std::endl;
        
        std::cout << "\nFile I/O test successful!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "File I/O error: " << e.what() << std::endl;
    }

    std::cout << "\n=== Demo completed successfully! ===" << std::endl;

    return 0;
}
