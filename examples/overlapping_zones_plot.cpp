#include "zoneout/zoneout.hpp"
#include <iostream>

namespace dp = datapod;

/**
 * Example: Overlapping Zones with Different Grid Resolutions in a Plot
 *
 * This example demonstrates:
 * 1. Creating a Plot to manage multiple zones
 * 2. Creating three zones with the same boundary but different grid resolutions
 * 3. Two zones (0.5m and 1.0m) overlap exactly (same boundary)
 * 4. One zone (2.0m) has a slightly different boundary
 * 5. Saving zones as GeoTIFF and GeoJSON via Plot
 * 6. Loading the Plot back and verifying the data
 *
 * The key feature is that Plot can manage multiple zones that represent
 * the same physical area at different resolutions, which is useful for:
 * - Multi-resolution analysis (coarse planning, fine execution)
 * - Different data layers requiring different resolutions
 * - Comparing zone representations at various scales
 */

int main() {
    std::cout << "=== Overlapping Zones Plot Example ===" << std::endl;

    // Create a GPS datum (example location: Netherlands)
    dp::Geo datum{52.0, 5.0, 0.0}; // Lat, Lon, Alt
    std::cout << "Using datum: lat=" << datum.latitude << ", lon=" << datum.longitude << ", alt=" << datum.altitude
              << std::endl;

    // Create a shared boundary for zones 1 and 2 (100m × 50m field)
    dp::Polygon boundary_exact;
    boundary_exact.vertices.push_back(dp::Point{0.0, 0.0, 0.0});
    boundary_exact.vertices.push_back(dp::Point{100.0, 0.0, 0.0});
    boundary_exact.vertices.push_back(dp::Point{100.0, 50.0, 0.0});
    boundary_exact.vertices.push_back(dp::Point{0.0, 50.0, 0.0});
    std::cout << "Created exact boundary: " << boundary_exact.vertices.size()
              << " points, area=" << boundary_exact.area() << " m²" << std::endl;

    // Create a slightly different boundary for zone 3 (90m × 45m field)
    dp::Polygon boundary_different;
    boundary_different.vertices.push_back(dp::Point{5.0, 5.0, 0.0});
    boundary_different.vertices.push_back(dp::Point{95.0, 5.0, 0.0});
    boundary_different.vertices.push_back(dp::Point{95.0, 50.0, 0.0});
    boundary_different.vertices.push_back(dp::Point{5.0, 50.0, 0.0});
    std::cout << "Created different boundary: " << boundary_different.vertices.size()
              << " points, area=" << boundary_different.area() << " m²" << std::endl;

    // ========== Create Plot ==========
    std::cout << std::endl << "--- Creating Plot ---" << std::endl;
    zoneout::Plot plot("Multi-Resolution Farm", "agricultural", datum);
    plot.set_property("farm_name", "Demo Farm");
    plot.set_property("location", "Netherlands");
    plot.set_property("year", "2024");
    std::cout << "Created plot: " << plot.name() << " (" << plot.type() << ")" << std::endl;

    // ========== Zone 1: High Resolution (0.5m) - Exact Boundary ==========
    std::cout << std::endl << "--- Creating Zone 1: High Resolution (0.5m) ---" << std::endl;
    zoneout::Zone zone_high_res("field_high_res", "agricultural", boundary_exact, datum, 0.5);
    zone_high_res.set_property("resolution", "0.5m");
    zone_high_res.set_property("crop", "wheat");
    zone_high_res.set_property("use_case", "precision_planting");

    std::cout << "Zone 1 - Name: " << zone_high_res.name() << std::endl;
    std::cout << "Zone 1 - Type: " << zone_high_res.type() << std::endl;
    std::cout << "Zone 1 - Resolution: " << zone_high_res.property("resolution").value_or("") << std::endl;
    std::cout << "Zone 1 - " << zone_high_res.raster_info() << std::endl;
    std::cout << "Zone 1 - Boundary area: " << zone_high_res.poly().area() << " m²" << std::endl;

    // Add a feature to high-res zone (obstacle)
    dp::Polygon obstacle_high;
    obstacle_high.vertices.push_back(dp::Point{20.0, 20.0, 0.0});
    obstacle_high.vertices.push_back(dp::Point{30.0, 20.0, 0.0});
    obstacle_high.vertices.push_back(dp::Point{30.0, 30.0, 0.0});
    obstacle_high.vertices.push_back(dp::Point{20.0, 30.0, 0.0});
    zone_high_res.add_polygon_element(obstacle_high, "tree_cluster", "obstacle", "vegetation");
    std::cout << "Zone 1 - " << zone_high_res.element_info() << std::endl;

    plot.add_zone(zone_high_res);
    std::cout << "Added Zone 1 to plot" << std::endl;

    // ========== Zone 2: Medium Resolution (1.0m) - Exact Boundary ==========
    std::cout << std::endl << "--- Creating Zone 2: Medium Resolution (1.0m) ---" << std::endl;
    zoneout::Zone zone_medium_res("field_medium_res", "agricultural", boundary_exact, datum, 1.0);
    zone_medium_res.set_property("resolution", "1.0m");
    zone_medium_res.set_property("crop", "wheat");
    zone_medium_res.set_property("use_case", "navigation_planning");

    std::cout << "Zone 2 - Name: " << zone_medium_res.name() << std::endl;
    std::cout << "Zone 2 - Type: " << zone_medium_res.type() << std::endl;
    std::cout << "Zone 2 - Resolution: " << zone_medium_res.property("resolution").value_or("") << std::endl;
    std::cout << "Zone 2 - " << zone_medium_res.raster_info() << std::endl;
    std::cout << "Zone 2 - Boundary area: " << zone_medium_res.poly().area() << " m²" << std::endl;

    // Add same obstacle to medium-res zone for comparison
    zone_medium_res.add_polygon_element(obstacle_high, "tree_cluster", "obstacle", "vegetation");
    std::cout << "Zone 2 - " << zone_medium_res.element_info() << std::endl;

    plot.add_zone(zone_medium_res);
    std::cout << "Added Zone 2 to plot" << std::endl;

    // ========== Zone 3: Low Resolution (2.0m) - Different Boundary ==========
    std::cout << std::endl << "--- Creating Zone 3: Low Resolution (2.0m) ---" << std::endl;
    zoneout::Zone zone_low_res("field_low_res", "agricultural", boundary_different, datum, 2.0);
    zone_low_res.set_property("resolution", "2.0m");
    zone_low_res.set_property("crop", "corn");
    zone_low_res.set_property("use_case", "yield_estimation");

    std::cout << "Zone 3 - Name: " << zone_low_res.name() << std::endl;
    std::cout << "Zone 3 - Type: " << zone_low_res.type() << std::endl;
    std::cout << "Zone 3 - Resolution: " << zone_low_res.property("resolution").value_or("") << std::endl;
    std::cout << "Zone 3 - " << zone_low_res.raster_info() << std::endl;
    std::cout << "Zone 3 - Boundary area: " << zone_low_res.poly().area() << " m²" << std::endl;

    // Add a different feature to low-res zone
    dp::Polygon obstacle_low;
    obstacle_low.vertices.push_back(dp::Point{60.0, 25.0, 0.0});
    obstacle_low.vertices.push_back(dp::Point{70.0, 25.0, 0.0});
    obstacle_low.vertices.push_back(dp::Point{70.0, 35.0, 0.0});
    obstacle_low.vertices.push_back(dp::Point{60.0, 35.0, 0.0});
    zone_low_res.add_polygon_element(obstacle_low, "building", "obstacle", "structure");
    std::cout << "Zone 3 - " << zone_low_res.element_info() << std::endl;

    plot.add_zone(zone_low_res);
    std::cout << "Added Zone 3 to plot" << std::endl;

    // ========== Save Plot to Directory ==========
    std::cout << std::endl << "--- Saving Plot to Directory ---" << std::endl;
    std::filesystem::path save_dir = "overlapping_zones_plot";
    std::filesystem::remove_all(save_dir); // Clean up if exists

    plot.save(save_dir);
    std::cout << "Plot saved to: " << save_dir.string() << std::endl;
    std::cout << "Total zones in plot: " << plot.zone_count() << std::endl;

    // List the saved files
    std::cout << std::endl << "Saved files structure:" << std::endl;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(save_dir)) {
        if (entry.is_regular_file()) {
            auto relative = std::filesystem::relative(entry.path(), save_dir);
            std::cout << "  - " << relative.string() << std::endl;
        }
    }

    // ========== Save Plot as TAR Archive ==========
    std::cout << std::endl << "--- Saving Plot as TAR Archive ---" << std::endl;
    std::filesystem::path tar_file = "overlapping_zones_plot.tar";
    std::filesystem::remove(tar_file); // Clean up if exists

    plot.save_tar(tar_file);
    std::cout << "Plot saved as TAR archive: " << tar_file.string() << std::endl;
    std::cout << "Archive size: " << std::filesystem::file_size(tar_file) << " bytes" << std::endl;

    // ========== Load Plot from Directory ==========
    std::cout << std::endl << "--- Loading Plot from Directory ---" << std::endl;
    auto loaded_plot = zoneout::Plot::load(save_dir, "Multi-Resolution Farm", "agricultural", datum);

    std::cout << "Loaded plot: " << loaded_plot.name() << " (" << loaded_plot.type() << ")" << std::endl;
    std::cout << "Total zones loaded: " << loaded_plot.zone_count() << std::endl;
    std::cout << "Farm name property: " << loaded_plot.property("farm_name").value_or("") << std::endl;

    // Verify loaded zones
    std::cout << std::endl << "--- Verifying Loaded Zones ---" << std::endl;
    const auto &loaded_zones = loaded_plot.zones();

    for (size_t i = 0; i < loaded_zones.size(); ++i) {
        const auto &zone = loaded_zones[i];
        std::cout << std::endl << "Loaded Zone " << i << ":" << std::endl;
        std::cout << "  Name: " << zone.name() << std::endl;
        std::cout << "  Type: " << zone.type() << std::endl;
        std::cout << "  Resolution: " << zone.property("resolution").value_or("") << std::endl;
        std::cout << "  Use case: " << zone.property("use_case").value_or("") << std::endl;
        std::cout << "  Crop: " << zone.property("crop").value_or("") << std::endl;
        std::cout << "  Raster: " << zone.raster_info() << std::endl;
        std::cout << "  Features: " << zone.element_info() << std::endl;
        std::cout << "  Boundary area: " << zone.poly().area() << " m²" << std::endl;
        std::cout << "  Has field boundary: " << (zone.poly().has_field_boundary() ? "true" : "false") << std::endl;
    }

    // ========== Load Plot from TAR Archive ==========
    std::cout << std::endl << "--- Loading Plot from TAR Archive ---" << std::endl;
    auto loaded_plot_tar = zoneout::Plot::load_tar(tar_file, "Multi-Resolution Farm", "agricultural", datum);

    std::cout << "Loaded plot from TAR: " << loaded_plot_tar.name() << " (" << loaded_plot_tar.type() << ")"
              << std::endl;
    std::cout << "Total zones loaded from TAR: " << loaded_plot_tar.zone_count() << std::endl;

    // ========== Compare Zone Resolutions ==========
    std::cout << std::endl << "--- Zone Resolution Comparison ---" << std::endl;
    const auto &zones = loaded_plot.zones();

    if (zones.size() >= 3) {
        std::cout << "Demonstrating multi-resolution analysis:" << std::endl;
        std::cout << "  Zone 0 (High-res): " << zones[0].raster_info() << std::endl;
        std::cout << "  Zone 1 (Med-res):  " << zones[1].raster_info() << std::endl;
        std::cout << "  Zone 2 (Low-res):  " << zones[2].raster_info() << std::endl;

        // Check if zones 0 and 1 have the same boundary (they should)
        double area0 = zones[0].poly().area();
        double area1 = zones[1].poly().area();
        double area2 = zones[2].poly().area();

        std::cout << std::endl << "Boundary overlap analysis:" << std::endl;
        std::cout << "  Zone 0 area: " << area0 << " m²" << std::endl;
        std::cout << "  Zone 1 area: " << area1 << " m²" << std::endl;
        std::cout << "  Zone 2 area: " << area2 << " m²" << std::endl;

        if (std::abs(area0 - area1) < 1.0) {
            std::cout << "  → Zones 0 and 1 have IDENTICAL boundaries (overlap exactly)" << std::endl;
        }

        if (std::abs(area0 - area2) > 1.0) {
            std::cout << "  → Zone 2 has a DIFFERENT boundary (does not overlap exactly)" << std::endl;
        }
    }

    // ========== Summary ==========
    std::cout << std::endl << "=== Summary ===" << std::endl;
    std::cout << "✓ Created a Plot with 3 zones" << std::endl;
    std::cout << "✓ Two zones (0.5m and 1.0m) share the exact same boundary" << std::endl;
    std::cout << "✓ One zone (2.0m) has a different boundary" << std::endl;
    std::cout << "✓ All zones saved as GeoJSON (vector) and GeoTIFF (raster)" << std::endl;
    std::cout << "✓ Plot saved both as directory and TAR archive" << std::endl;
    std::cout << "✓ Successfully loaded and verified all data" << std::endl;
    std::cout << std::endl << "Output files:" << std::endl;
    std::cout << "  - Directory: " << save_dir.string() << std::endl;
    std::cout << "  - TAR file:  " << tar_file.string() << std::endl;

    return 0;
}
