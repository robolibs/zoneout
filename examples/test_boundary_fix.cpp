#include <iostream>
#include "zoneout/zoneout.hpp"

int main() {
    // Create a simple test zone
    concord::Datum datum{51.73019, 4.23883, 0.0};
    
    // Create a simple square boundary
    concord::Polygon boundary;
    boundary.addPoint({0, 0, 0});
    boundary.addPoint({100, 0, 0});
    boundary.addPoint({100, 100, 0});
    boundary.addPoint({0, 100, 0});
    
    // Create zone with boundary
    zoneout::Zone zone("TestField", "field", boundary, datum, 10.0);
    
    // Add one polygon feature
    concord::Polygon feature;
    feature.addPoint({20, 20, 0});
    feature.addPoint({40, 20, 0});
    feature.addPoint({40, 40, 0});
    feature.addPoint({20, 40, 0});
    
    zone.addPolygonFeature(feature, "TestFeature", "crop", "wheat");
    
    // Save the zone
    zone.toFiles("test_boundary.json", "test_boundary.tif");
    
    // Check what's in the saved file
    std::cout << "Zone saved. Check test_boundary.json for output." << std::endl;
    std::cout << "Expected: 2 features (1 boundary with border:true, 1 crop feature with border:false)" << std::endl;
    
    // Load and check
    auto loaded_zone = zoneout::Zone::fromFiles("test_boundary.json", "test_boundary.tif");
    std::cout << "Polygon elements after loading: " << loaded_zone.poly_data_.getPolygonElements().size() << std::endl;
    std::cout << "Has field boundary: " << loaded_zone.poly_data_.hasFieldBoundary() << std::endl;
    
    return 0;
}