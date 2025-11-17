#include "zoneout/zoneout.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

/**
 * Example: Builder Pattern for Zone and Plot Construction
 *
 * This example demonstrates the fluent Builder pattern for creating
 * Zones and Plots in a readable, maintainable way.
 *
 * Benefits of Builder pattern:
 * - Expressive, self-documenting code
 * - Optional parameters with clear defaults
 * - Validation before object construction
 * - Method chaining for fluent interface
 * - Easy to extend with new options
 */

// Helper function to create a simple rectangular boundary
concord::Polygon create_rectangle(double width, double height, double offset_x = 0.0, double offset_y = 0.0) {
    concord::Polygon rect;
    rect.addPoint(concord::Point{offset_x, offset_y, 0.0});
    rect.addPoint(concord::Point{offset_x + width, offset_y, 0.0});
    rect.addPoint(concord::Point{offset_x + width, offset_y + height, 0.0});
    rect.addPoint(concord::Point{offset_x, offset_y + height, 0.0});
    return rect;
}

// Helper function to create an obstacle polygon
concord::Polygon create_obstacle(double x, double y, double size) {
    concord::Polygon obstacle;
    obstacle.addPoint(concord::Point{x, y, 0.0});
    obstacle.addPoint(concord::Point{x + size, y, 0.0});
    obstacle.addPoint(concord::Point{x + size, y + size, 0.0});
    obstacle.addPoint(concord::Point{x, y + size, 0.0});
    return obstacle;
}

int main() {
    spdlog::info("=== Builder Pattern Example ===\n");

    // Common datum for all examples
    concord::Datum datum{52.0, 5.0, 0.0}; // Netherlands

    // ========================================================================
    // Example 1: Basic ZoneBuilder Usage
    // ========================================================================
    spdlog::info("--- Example 1: Basic ZoneBuilder ---");

    try {
        auto zone1 = zoneout::ZoneBuilder()
                         .with_name("wheat_field")
                         .with_type("agricultural")
                         .with_boundary(create_rectangle(100.0, 50.0))
                         .with_datum(datum)
                         .with_resolution(1.0)
                         .with_property("crop", "wheat")
                         .with_property("season", "2024")
                         .build();

        spdlog::info("✓ Created zone: {} ({})", zone1.name(), zone1.type());
        spdlog::info("  Resolution: 1.0m");
        spdlog::info("  Crop: {}", zone1.get_property("crop"));
        spdlog::info("  {}", zone1.raster_info());
    } catch (const std::exception &e) {
        spdlog::error("✗ Failed to build zone: {}", e.what());
    }

    // ========================================================================
    // Example 2: ZoneBuilder with Features and Layers
    // ========================================================================
    spdlog::info("\n--- Example 2: ZoneBuilder with Features ---");

    try {
        // Create a high-resolution zone with obstacles
        auto zone2 = zoneout::ZoneBuilder()
                         .with_name("precision_field")
                         .with_type("agricultural")
                         .with_boundary(create_rectangle(80.0, 60.0))
                         .with_datum(datum)
                         .with_resolution(0.5) // High resolution
                         .with_property("crop", "corn")
                         .with_property("irrigation", "drip")
                         .with_property("soil_type", "loamy")
                         .with_polygon_feature(create_obstacle(20.0, 20.0, 10.0), "tree_1", "obstacle", "vegetation")
                         .with_polygon_feature(create_obstacle(50.0, 30.0, 8.0), "building", "obstacle", "structure")
                         .build();

        spdlog::info("✓ Created zone: {} ({})", zone2.name(), zone2.type());
        spdlog::info("  Resolution: 0.5m (high precision)");
        spdlog::info("  {}", zone2.raster_info());
        spdlog::info("  {}", zone2.feature_info());
    } catch (const std::exception &e) {
        spdlog::error("✗ Failed to build zone: {}", e.what());
    }

    // ========================================================================
    // Example 3: PlotBuilder with Pre-built Zones
    // ========================================================================
    spdlog::info("\n--- Example 3: PlotBuilder with Pre-built Zones ---");

    try {
        // Build zones separately
        auto field1 = zoneout::ZoneBuilder()
                          .with_name("north_field")
                          .with_type("agricultural")
                          .with_boundary(create_rectangle(100.0, 50.0, 0.0, 60.0))
                          .with_datum(datum)
                          .with_resolution(1.0)
                          .with_property("crop", "wheat")
                          .build();

        auto field2 = zoneout::ZoneBuilder()
                          .with_name("south_field")
                          .with_type("agricultural")
                          .with_boundary(create_rectangle(100.0, 50.0, 0.0, 0.0))
                          .with_datum(datum)
                          .with_resolution(1.0)
                          .with_property("crop", "barley")
                          .build();

        // Build plot with pre-built zones
        auto plot1 = zoneout::PlotBuilder()
                         .with_name("Twin Fields Farm")
                         .with_type("agricultural")
                         .with_datum(datum)
                         .with_property("farm_owner", "Demo Farms Inc.")
                         .with_property("location", "Netherlands")
                         .add_zone(field1)
                         .add_zone(field2)
                         .build();

        spdlog::info("✓ Created plot: {} ({})", plot1.get_name(), plot1.get_type());
        spdlog::info("  Total zones: {}", plot1.get_zone_count());
        spdlog::info("  Owner: {}", plot1.get_property("farm_owner"));

        for (const auto &zone : plot1.get_zones()) {
            spdlog::info("    - {} ({}, crop: {})", zone.name(), zone.type(), zone.get_property("crop"));
        }
    } catch (const std::exception &e) {
        spdlog::error("✗ Failed to build plot: {}", e.what());
    }

    // ========================================================================
    // Example 4: PlotBuilder with Inline Zone Construction (Lambda)
    // ========================================================================
    spdlog::info("\n--- Example 4: PlotBuilder with Inline Zones ---");

    try {
        // Build plot with zones constructed inline using lambda configurators
        auto plot2 = zoneout::PlotBuilder()
                         .with_name("Multi-Resolution Farm")
                         .with_type("agricultural")
                         .with_datum(datum)
                         .with_property("farm_type", "research")
                         .with_property("established", "2024")
                         // Zone 1: High resolution
                         .add_zone([](zoneout::ZoneBuilder &builder) {
                             builder.with_name("high_res_zone")
                                 .with_type("experimental")
                                 .with_boundary(create_rectangle(50.0, 50.0, 0.0, 0.0))
                                 .with_resolution(0.25) // Very high resolution
                                 .with_property("experiment", "nitrogen_study")
                                 .with_property("plot_id", "A1")
                                 .with_polygon_feature(create_obstacle(10.0, 10.0, 5.0), "sensor_station", "equipment",
                                                       "sensor");
                         })
                         // Zone 2: Medium resolution
                         .add_zone([](zoneout::ZoneBuilder &builder) {
                             builder.with_name("medium_res_zone")
                                 .with_type("production")
                                 .with_boundary(create_rectangle(100.0, 100.0, 60.0, 0.0))
                                 .with_resolution(1.0)
                                 .with_property("crop", "wheat")
                                 .with_property("variety", "spring_wheat");
                         })
                         // Zone 3: Low resolution (overview)
                         .add_zone([](zoneout::ZoneBuilder &builder) {
                             builder.with_name("overview_zone")
                                 .with_type("monitoring")
                                 .with_boundary(create_rectangle(200.0, 150.0, 0.0, 60.0))
                                 .with_resolution(5.0) // Coarse resolution for overview
                                 .with_property("purpose", "aerial_monitoring");
                         })
                         .build();

        spdlog::info("✓ Created plot: {} ({})", plot2.get_name(), plot2.get_type());
        spdlog::info("  Total zones: {}", plot2.get_zone_count());
        spdlog::info("  Farm type: {}", plot2.get_property("farm_type"));

        for (const auto &zone : plot2.get_zones()) {
            spdlog::info("    - {} ({}) - {}", zone.name(), zone.type(), zone.raster_info());
        }
    } catch (const std::exception &e) {
        spdlog::error("✗ Failed to build plot: {}", e.what());
    }

    // ========================================================================
    // Example 5: Complex Multi-Resolution Plot with All Features
    // ========================================================================
    spdlog::info("\n--- Example 5: Complex Multi-Resolution Plot ---");

    try {
        // This demonstrates the most advanced use case:
        // - Multiple zones with different resolutions
        // - Mix of pre-built and inline zones
        // - Properties at both plot and zone levels
        // - Features and obstacles

        // Pre-build a barn zone
        auto barn_zone = zoneout::ZoneBuilder()
                             .with_name("barn_area")
                             .with_type("infrastructure")
                             .with_boundary(create_rectangle(30.0, 20.0, 170.0, 0.0))
                             .with_datum(datum)
                             .with_resolution(0.1) // Very high resolution for building
                             .with_property("structure_type", "barn")
                             .with_property("capacity", "50_cattle")
                             .build();

        auto complex_plot =
            zoneout::PlotBuilder()
                .with_name("Complete Research Farm")
                .with_type("agricultural")
                .with_datum(datum)
                .with_properties({{"owner", "AgriTech Labs"}, {"location", "Wageningen, NL"}, {"year", "2024"}})
                // Add pre-built barn zone
                .add_zone(barn_zone)
                // Add grazing field with inline construction
                .add_zone([](zoneout::ZoneBuilder &builder) {
                    builder.with_name("grazing_field")
                        .with_type("pasture")
                        .with_boundary(create_rectangle(150.0, 100.0, 0.0, 0.0))
                        .with_resolution(2.0) // Coarse resolution for large area
                        .with_property("grass_type", "mixed")
                        .with_property("animals", "cattle")
                        .with_polygon_feature(create_obstacle(50.0, 50.0, 15.0), "water_trough", "utility", "water")
                        .with_polygon_feature(create_obstacle(100.0, 60.0, 10.0), "shade_structure", "shelter",
                                              "building");
                })
                // Add crop trial zone with high precision
                .add_zone([](zoneout::ZoneBuilder &builder) {
                    builder.with_name("trial_plots")
                        .with_type("experimental")
                        .with_boundary(create_rectangle(60.0, 80.0, 160.0, 30.0))
                        .with_resolution(0.2) // Very high resolution for trials
                        .with_property("experiment_id", "EXP-2024-001")
                        .with_property("crop_varieties", "12")
                        .with_polygon_feature(create_obstacle(170.0, 40.0, 3.0), "weather_station", "sensor",
                                              "meteorological");
                })
                .build();

        spdlog::info("✓ Created complex plot: {} ({})", complex_plot.get_name(), complex_plot.get_type());
        spdlog::info("  Total zones: {}", complex_plot.get_zone_count());
        spdlog::info("  Owner: {}", complex_plot.get_property("owner"));
        spdlog::info("  Location: {}", complex_plot.get_property("location"));

        spdlog::info("\n  Zone details:");
        for (const auto &zone : complex_plot.get_zones()) {
            spdlog::info("    - {} ({})", zone.name(), zone.type());
            spdlog::info("      {}", zone.raster_info());
            spdlog::info("      {}", zone.feature_info());
        }

        // Save the complex plot
        std::filesystem::path save_dir = "complex_builder_plot";
        std::filesystem::remove_all(save_dir);
        complex_plot.save(save_dir);
        spdlog::info("\n✓ Saved complex plot to: {}", save_dir.string());

    } catch (const std::exception &e) {
        spdlog::error("✗ Failed to build complex plot: {}", e.what());
    }

    // ========================================================================
    // Example 6: Validation and Error Handling
    // ========================================================================
    spdlog::info("\n--- Example 6: Validation and Error Handling ---");

    // Test validation with missing required fields
    zoneout::ZoneBuilder invalid_builder;
    invalid_builder.with_name("incomplete_zone").with_type("test").with_datum(datum);
    // Missing boundary!

    if (!invalid_builder.is_valid()) {
        spdlog::warn("✗ Builder validation failed (as expected): {}", invalid_builder.validation_error());
    }

    // Attempting to build will throw
    try {
        auto invalid_zone = invalid_builder.build();
        spdlog::error("Should not reach here!");
    } catch (const std::invalid_argument &e) {
        spdlog::info("✓ Build correctly threw exception: {}", e.what());
    }

    // ========================================================================
    // Example 7: Builder Reset and Reuse
    // ========================================================================
    spdlog::info("\n--- Example 7: Builder Reset and Reuse ---");

    zoneout::ZoneBuilder reusable_builder;

    // Build first zone
    auto zone_v1 = reusable_builder.with_name("zone_v1")
                       .with_type("agricultural")
                       .with_boundary(create_rectangle(50.0, 50.0))
                       .with_datum(datum)
                       .with_resolution(1.0)
                       .build();

    spdlog::info("✓ Built zone: {}", zone_v1.name());

    // Reset and reuse for different zone
    reusable_builder.reset();

    auto zone_v2 = reusable_builder.with_name("zone_v2")
                       .with_type("pasture")
                       .with_boundary(create_rectangle(75.0, 40.0))
                       .with_datum(datum)
                       .with_resolution(2.0)
                       .with_property("grass", "clover")
                       .build();

    spdlog::info("✓ Built zone after reset: {}", zone_v2.name());

    // ========================================================================
    // Summary
    // ========================================================================
    spdlog::info("\n=== Summary ===");
    spdlog::info("✓ Demonstrated ZoneBuilder for fluent zone construction");
    spdlog::info("✓ Demonstrated PlotBuilder with pre-built and inline zones");
    spdlog::info("✓ Showed validation and error handling");
    spdlog::info("✓ Demonstrated builder reset and reuse");
    spdlog::info("✓ Created complex multi-resolution plots with features");
    spdlog::info("\nBuilder patterns make code more:");
    spdlog::info("  - Readable: Self-documenting method names");
    spdlog::info("  - Maintainable: Easy to add/modify options");
    spdlog::info("  - Safe: Validation before construction");
    spdlog::info("  - Flexible: Optional parameters with defaults");

    return 0;
}
