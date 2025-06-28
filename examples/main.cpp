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
    for (const auto& [uuid, name] : zone_names) {
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
    std::cout << "Time until then: " << zoneout::time_utils::durationToString(
        zoneout::time_utils::timeUntil(future_time)) << std::endl;
    
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
    
    std::cout << "\n=== Demo completed successfully! ===" << std::endl;
    
    return 0;
}
