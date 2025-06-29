#include "include/zoneout/zoneout.hpp"

int main() {
    // Test new enhanced Poly with subtype
    zoneout::Poly poly("Test Field", "agricultural", "crop");
    
    // Test structured element addition
    auto uuid = zoneout::generateUUID();
    concord::Polygon zone_poly({{0,0,0}, {10,0,0}, {10,10,0}, {0,10,0}, {0,0,0}});
    poly.addPolygonElement(uuid, "Zone A", "zone", "planting", zone_poly);
    
    // Test access
    auto polygons = poly.getPolygonElements();
    auto zones = poly.getPolygonsByType("zone");
    
    std::cout << "Poly has " << polygons.size() << " polygon elements" << std::endl;
    std::cout << "Found " << zones.size() << " zone polygons" << std::endl;
    
    // Test enhanced Grid
    zoneout::Grid grid("Test Grid", "elevation", "dem");
    
    // Test Zone with enhanced components
    // Wageningen Research Labs coordinates
    const concord::Datum WAGENINGEN_DATUM{51.98776171041831, 5.662378206146002, 0.0};
    
    zoneout::Zone zone("Test Zone", "field", WAGENINGEN_DATUM);
    
    return 0;
}