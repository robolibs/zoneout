#include <iostream>
#include "zoneout/zoneout.hpp"

using namespace zoneout;

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
    std::cout << "=== Zone Spaces Example ===" << std::endl;
    
    // Create a zone (warehouse facility)
    auto warehouse_boundary = createRectangle(0, 0, 200, 100);
    Zone warehouse = Zone::createBarn("Warehouse Facility", warehouse_boundary);
    
    std::cout << "Created warehouse zone: " << warehouse.getName() << std::endl;
    std::cout << "Total area: " << warehouse.area() << " m²" << std::endl;
    
    // Add different types of spaces within the warehouse
    
    // 1. Loading dock space
    auto loading_dock = createRectangle(5, 5, 40, 15);
    std::unordered_map<std::string, std::string> loading_props;
    loading_props["name"] = "loading_dock_1";
    loading_props["purpose"] = "material_handling";
    loading_props["capacity"] = "5_trucks";
    loading_props["height_clearance"] = "4.5m";
    warehouse.add_element(loading_dock, "space", loading_props);
    
    // 2. Storage area
    auto storage_area = createRectangle(50, 10, 80, 60);
    std::unordered_map<std::string, std::string> storage_props;
    storage_props["name"] = "dry_storage";
    storage_props["purpose"] = "equipment_storage";
    storage_props["climate_control"] = "no";
    storage_props["max_weight"] = "1000_kg_per_m2";
    warehouse.add_element(storage_area, "space", storage_props);
    
    // 3. Cold storage space
    auto cold_storage = createRectangle(140, 10, 50, 40);
    std::unordered_map<std::string, std::string> cold_props;
    cold_props["name"] = "cold_storage";
    cold_props["purpose"] = "temperature_controlled";
    cold_props["temperature"] = "2-8_celsius";
    cold_props["humidity"] = "85_percent";
    warehouse.add_element(cold_storage, "space", cold_props);
    
    // 4. Office space
    auto office_space = createRectangle(140, 60, 50, 30);
    std::unordered_map<std::string, std::string> office_props;
    office_props["name"] = "admin_office";
    office_props["purpose"] = "administration";
    office_props["occupancy"] = "10_people";
    office_props["hvac"] = "yes";
    warehouse.add_element(office_space, "space", office_props);
    
    // 5. Vehicle parking space
    auto parking_space = createRectangle(10, 75, 120, 20);
    std::unordered_map<std::string, std::string> parking_props;
    parking_props["name"] = "vehicle_parking";
    parking_props["purpose"] = "equipment_parking";
    parking_props["capacity"] = "8_vehicles";
    parking_props["surface"] = "concrete";
    warehouse.add_element(parking_space, "space", parking_props);
    
    std::cout << "\nAdded " << warehouse.get_elements("space").size() << " spaces to the warehouse" << std::endl;
    
    // Query and display all spaces
    std::cout << "\n=== Spaces in Warehouse ===" << std::endl;
    auto spaces = warehouse.get_elements("space");
    
    for (const auto& space : spaces) {
        auto name_it = space.properties.find("name");
        auto purpose_it = space.properties.find("purpose");
        
        if (name_it != space.properties.end()) {
            std::cout << "📍 " << name_it->second;
            if (purpose_it != space.properties.end()) {
                std::cout << " (" << purpose_it->second << ")";
            }
            std::cout << std::endl;
            
            // Show key properties
            for (const auto& [key, value] : space.properties) {
                if (key != "name" && key != "purpose") {
                    std::cout << "   " << key << ": " << value << std::endl;
                }
            }
            std::cout << std::endl;
        }
    }
    
    // Query specific types of spaces
    std::cout << "=== Space Analytics ===" << std::endl;
    
    int storage_spaces = 0;
    int parking_spaces = 0;
    int office_spaces = 0;
    
    for (const auto& space : spaces) {
        auto purpose_it = space.properties.find("purpose");
        if (purpose_it != space.properties.end()) {
            if (purpose_it->second.find("storage") != std::string::npos) {
                storage_spaces++;
            } else if (purpose_it->second.find("parking") != std::string::npos) {
                parking_spaces++;
            } else if (purpose_it->second.find("administration") != std::string::npos) {
                office_spaces++;
            }
        }
    }
    
    std::cout << "Storage spaces: " << storage_spaces << std::endl;
    std::cout << "Parking spaces: " << parking_spaces << std::endl;
    std::cout << "Office spaces: " << office_spaces << std::endl;
    
    // Test point-in-space queries
    std::cout << "\n=== Point Queries ===" << std::endl;
    concord::Point test_point(100, 30, 0);
    
    std::cout << "Testing point (100, 30):" << std::endl;
    if (warehouse.contains(test_point)) {
        std::cout << "✓ Point is within warehouse boundary" << std::endl;
        
        // Check which space it's in
        for (const auto& space : spaces) {
            if (std::holds_alternative<concord::Polygon>(space.geometry)) {
                auto space_polygon = std::get<concord::Polygon>(space.geometry);
                if (space_polygon.contains(test_point)) {
                    auto name_it = space.properties.find("name");
                    if (name_it != space.properties.end()) {
                        std::cout << "✓ Point is in space: " << name_it->second << std::endl;
                    }
                    break;
                }
            }
        }
    } else {
        std::cout << "✗ Point is outside warehouse boundary" << std::endl;
    }
    
    return 0;
}