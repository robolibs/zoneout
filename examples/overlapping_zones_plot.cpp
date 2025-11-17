#include "zoneout/zoneout.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

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
    spdlog::info("=== Overlapping Zones Plot Example ===");

    // Create a GPS datum (example location: Netherlands)
    concord::Datum datum{52.0, 5.0, 0.0}; // Lat, Lon, Alt
    spdlog::info("Using datum: lat={}, lon={}, alt={}", datum.lat, datum.lon, datum.alt);

    // Create a shared boundary for zones 1 and 2 (100m × 50m field)
    concord::Polygon boundary_exact;
    boundary_exact.addPoint(concord::Point{0.0, 0.0, 0.0});
    boundary_exact.addPoint(concord::Point{100.0, 0.0, 0.0});
    boundary_exact.addPoint(concord::Point{100.0, 50.0, 0.0});
    boundary_exact.addPoint(concord::Point{0.0, 50.0, 0.0});
    spdlog::info("Created exact boundary: {} points, area={} m²", boundary_exact.getPoints().size(),
                 boundary_exact.area());

    // Create a slightly different boundary for zone 3 (90m × 45m field)
    concord::Polygon boundary_different;
    boundary_different.addPoint(concord::Point{5.0, 5.0, 0.0});
    boundary_different.addPoint(concord::Point{95.0, 5.0, 0.0});
    boundary_different.addPoint(concord::Point{95.0, 50.0, 0.0});
    boundary_different.addPoint(concord::Point{5.0, 50.0, 0.0});
    spdlog::info("Created different boundary: {} points, area={} m²", boundary_different.getPoints().size(),
                 boundary_different.area());

    // ========== Create Plot ==========
    spdlog::info("\n--- Creating Plot ---");
    zoneout::Plot plot("Multi-Resolution Farm", "agricultural", datum);
    plot.set_property("farm_name", "Demo Farm");
    plot.set_property("location", "Netherlands");
    plot.set_property("year", "2024");
    spdlog::info("Created plot: {} ({})", plot.get_name(), plot.get_type());

    // ========== Zone 1: High Resolution (0.5m) - Exact Boundary ==========
    spdlog::info("\n--- Creating Zone 1: High Resolution (0.5m) ---");
    zoneout::Zone zone_high_res("field_high_res", "agricultural", boundary_exact, datum, 0.5);
    zone_high_res.set_property("resolution", "0.5m");
    zone_high_res.set_property("crop", "wheat");
    zone_high_res.set_property("use_case", "precision_planting");

    spdlog::info("Zone 1 - Name: {}", zone_high_res.name());
    spdlog::info("Zone 1 - Type: {}", zone_high_res.type());
    spdlog::info("Zone 1 - Resolution: {}", zone_high_res.get_property("resolution"));
    spdlog::info("Zone 1 - {}", zone_high_res.raster_info());
    spdlog::info("Zone 1 - Boundary area: {} m²", zone_high_res.poly().area());

    // Add a feature to high-res zone (obstacle)
    concord::Polygon obstacle_high;
    obstacle_high.addPoint(concord::Point{20.0, 20.0, 0.0});
    obstacle_high.addPoint(concord::Point{30.0, 20.0, 0.0});
    obstacle_high.addPoint(concord::Point{30.0, 30.0, 0.0});
    obstacle_high.addPoint(concord::Point{20.0, 30.0, 0.0});
    zone_high_res.add_polygon_feature(obstacle_high, "tree_cluster", "obstacle", "vegetation");
    spdlog::info("Zone 1 - {}", zone_high_res.feature_info());

    plot.add_zone(zone_high_res);
    spdlog::info("Added Zone 1 to plot");

    // ========== Zone 2: Medium Resolution (1.0m) - Exact Boundary ==========
    spdlog::info("\n--- Creating Zone 2: Medium Resolution (1.0m) ---");
    zoneout::Zone zone_medium_res("field_medium_res", "agricultural", boundary_exact, datum, 1.0);
    zone_medium_res.set_property("resolution", "1.0m");
    zone_medium_res.set_property("crop", "wheat");
    zone_medium_res.set_property("use_case", "navigation_planning");

    spdlog::info("Zone 2 - Name: {}", zone_medium_res.name());
    spdlog::info("Zone 2 - Type: {}", zone_medium_res.type());
    spdlog::info("Zone 2 - Resolution: {}", zone_medium_res.get_property("resolution"));
    spdlog::info("Zone 2 - {}", zone_medium_res.raster_info());
    spdlog::info("Zone 2 - Boundary area: {} m²", zone_medium_res.poly().area());

    // Add same obstacle to medium-res zone for comparison
    zone_medium_res.add_polygon_feature(obstacle_high, "tree_cluster", "obstacle", "vegetation");
    spdlog::info("Zone 2 - {}", zone_medium_res.feature_info());

    plot.add_zone(zone_medium_res);
    spdlog::info("Added Zone 2 to plot");

    // ========== Zone 3: Low Resolution (2.0m) - Different Boundary ==========
    spdlog::info("\n--- Creating Zone 3: Low Resolution (2.0m) ---");
    zoneout::Zone zone_low_res("field_low_res", "agricultural", boundary_different, datum, 2.0);
    zone_low_res.set_property("resolution", "2.0m");
    zone_low_res.set_property("crop", "corn");
    zone_low_res.set_property("use_case", "yield_estimation");

    spdlog::info("Zone 3 - Name: {}", zone_low_res.name());
    spdlog::info("Zone 3 - Type: {}", zone_low_res.type());
    spdlog::info("Zone 3 - Resolution: {}", zone_low_res.get_property("resolution"));
    spdlog::info("Zone 3 - {}", zone_low_res.raster_info());
    spdlog::info("Zone 3 - Boundary area: {} m²", zone_low_res.poly().area());

    // Add a different feature to low-res zone
    concord::Polygon obstacle_low;
    obstacle_low.addPoint(concord::Point{60.0, 25.0, 0.0});
    obstacle_low.addPoint(concord::Point{70.0, 25.0, 0.0});
    obstacle_low.addPoint(concord::Point{70.0, 35.0, 0.0});
    obstacle_low.addPoint(concord::Point{60.0, 35.0, 0.0});
    zone_low_res.add_polygon_feature(obstacle_low, "building", "obstacle", "structure");
    spdlog::info("Zone 3 - {}", zone_low_res.feature_info());

    plot.add_zone(zone_low_res);
    spdlog::info("Added Zone 3 to plot");

    // ========== Save Plot to Directory ==========
    spdlog::info("\n--- Saving Plot to Directory ---");
    std::filesystem::path save_dir = "overlapping_zones_plot";
    std::filesystem::remove_all(save_dir); // Clean up if exists

    plot.save(save_dir);
    spdlog::info("Plot saved to: {}", save_dir.string());
    spdlog::info("Total zones in plot: {}", plot.get_zone_count());

    // List the saved files
    spdlog::info("\nSaved files structure:");
    for (const auto &entry : std::filesystem::recursive_directory_iterator(save_dir)) {
        if (entry.is_regular_file()) {
            auto relative = std::filesystem::relative(entry.path(), save_dir);
            spdlog::info("  - {}", relative.string());
        }
    }

    // ========== Save Plot as TAR Archive ==========
    spdlog::info("\n--- Saving Plot as TAR Archive ---");
    std::filesystem::path tar_file = "overlapping_zones_plot.tar";
    std::filesystem::remove(tar_file); // Clean up if exists

    plot.save_tar(tar_file);
    spdlog::info("Plot saved as TAR archive: {}", tar_file.string());
    spdlog::info("Archive size: {} bytes", std::filesystem::file_size(tar_file));

    // ========== Load Plot from Directory ==========
    spdlog::info("\n--- Loading Plot from Directory ---");
    auto loaded_plot = zoneout::Plot::load(save_dir, "Multi-Resolution Farm", "agricultural", datum);

    spdlog::info("Loaded plot: {} ({})", loaded_plot.get_name(), loaded_plot.get_type());
    spdlog::info("Total zones loaded: {}", loaded_plot.get_zone_count());
    spdlog::info("Farm name property: {}", loaded_plot.get_property("farm_name"));

    // Verify loaded zones
    spdlog::info("\n--- Verifying Loaded Zones ---");
    const auto &loaded_zones = loaded_plot.get_zones();

    for (size_t i = 0; i < loaded_zones.size(); ++i) {
        const auto &zone = loaded_zones[i];
        spdlog::info("\nLoaded Zone {}:", i);
        spdlog::info("  Name: {}", zone.name());
        spdlog::info("  Type: {}", zone.type());
        spdlog::info("  Resolution: {}", zone.get_property("resolution"));
        spdlog::info("  Use case: {}", zone.get_property("use_case"));
        spdlog::info("  Crop: {}", zone.get_property("crop"));
        spdlog::info("  Raster: {}", zone.raster_info());
        spdlog::info("  Features: {}", zone.feature_info());
        spdlog::info("  Boundary area: {:.2f} m²", zone.poly().area());
        spdlog::info("  Has field boundary: {}", zone.poly().has_field_boundary());
    }

    // ========== Load Plot from TAR Archive ==========
    spdlog::info("\n--- Loading Plot from TAR Archive ---");
    auto loaded_plot_tar = zoneout::Plot::load_tar(tar_file, "Multi-Resolution Farm", "agricultural", datum);

    spdlog::info("Loaded plot from TAR: {} ({})", loaded_plot_tar.get_name(), loaded_plot_tar.get_type());
    spdlog::info("Total zones loaded from TAR: {}", loaded_plot_tar.get_zone_count());

    // ========== Compare Zone Resolutions ==========
    spdlog::info("\n--- Zone Resolution Comparison ---");
    const auto &zones = loaded_plot.get_zones();

    if (zones.size() >= 3) {
        spdlog::info("Demonstrating multi-resolution analysis:");
        spdlog::info("  Zone 0 (High-res): {}", zones[0].raster_info());
        spdlog::info("  Zone 1 (Med-res):  {}", zones[1].raster_info());
        spdlog::info("  Zone 2 (Low-res):  {}", zones[2].raster_info());

        // Check if zones 0 and 1 have the same boundary (they should)
        double area0 = zones[0].poly().area();
        double area1 = zones[1].poly().area();
        double area2 = zones[2].poly().area();

        spdlog::info("\nBoundary overlap analysis:");
        spdlog::info("  Zone 0 area: {:.2f} m²", area0);
        spdlog::info("  Zone 1 area: {:.2f} m²", area1);
        spdlog::info("  Zone 2 area: {:.2f} m²", area2);

        if (std::abs(area0 - area1) < 1.0) {
            spdlog::info("  → Zones 0 and 1 have IDENTICAL boundaries (overlap exactly)");
        }

        if (std::abs(area0 - area2) > 1.0) {
            spdlog::info("  → Zone 2 has a DIFFERENT boundary (does not overlap exactly)");
        }
    }

    // ========== Summary ==========
    spdlog::info("\n=== Summary ===");
    spdlog::info("✓ Created a Plot with 3 zones");
    spdlog::info("✓ Two zones (0.5m and 1.0m) share the exact same boundary");
    spdlog::info("✓ One zone (2.0m) has a different boundary");
    spdlog::info("✓ All zones saved as GeoJSON (vector) and GeoTIFF (raster)");
    spdlog::info("✓ Plot saved both as directory and TAR archive");
    spdlog::info("✓ Successfully loaded and verified all data");
    spdlog::info("\nOutput files:");
    spdlog::info("  - Directory: {}", save_dir.string());
    spdlog::info("  - TAR file:  {}", tar_file.string());

    return 0;
}
