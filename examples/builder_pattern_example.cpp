#include "zoneout/zoneout.hpp"
#include <iostream>

namespace dp = datapod;

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
dp::Polygon create_rectangle(double width, double height, double offset_x = 0.0, double offset_y = 0.0) {
    dp::Polygon rect;
    rect.vertices.push_back(dp::Point{offset_x, offset_y, 0.0});
    rect.vertices.push_back(dp::Point{offset_x + width, offset_y, 0.0});
    rect.vertices.push_back(dp::Point{offset_x + width, offset_y + height, 0.0});
    rect.vertices.push_back(dp::Point{offset_x, offset_y + height, 0.0});
    return rect;
}

// Helper function to create an obstacle polygon
dp::Polygon create_obstacle(double x, double y, double size) {
    dp::Polygon obstacle;
    obstacle.vertices.push_back(dp::Point{x, y, 0.0});
    obstacle.vertices.push_back(dp::Point{x + size, y, 0.0});
    obstacle.vertices.push_back(dp::Point{x + size, y + size, 0.0});
    obstacle.vertices.push_back(dp::Point{x, y + size, 0.0});
    return obstacle;
}

int main() {
    std::cout << "=== Builder Pattern Example ===" << std::endl << std::endl;

    // Common datum for all examples
    dp::Geo datum{52.0, 5.0, 0.0}; // Netherlands

    // ========================================================================
    // Example 1: Basic ZoneBuilder Usage
    // ========================================================================
    std::cout << "--- Example 1: Basic ZoneBuilder ---" << std::endl;

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

        std::cout << "✓ Created zone: " << zone1.name() << " (" << zone1.type() << ")" << std::endl;
        std::cout << "  Resolution: 1.0m" << std::endl;
        std::cout << "  Crop: " << zone1.get_property("crop") << std::endl;
        std::cout << "  " << zone1.raster_info() << std::endl;
    } catch (const std::exception &e) {
        std::cout << "✗ Failed to build zone: " << e.what() << std::endl;
    }

    // ========================================================================
    // Example 2: ZoneBuilder with Features and Layers
    // ========================================================================
    std::cout << std::endl << "--- Example 2: ZoneBuilder with Features ---" << std::endl;

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

        std::cout << "✓ Created zone: " << zone2.name() << " (" << zone2.type() << ")" << std::endl;
        std::cout << "  Resolution: 0.5m (high precision)" << std::endl;
        std::cout << "  " << zone2.raster_info() << std::endl;
        std::cout << "  " << zone2.feature_info() << std::endl;
    } catch (const std::exception &e) {
        std::cout << "✗ Failed to build zone: " << e.what() << std::endl;
    }

    // ========================================================================
    // Example 3: PlotBuilder with Pre-built Zones
    // ========================================================================
    std::cout << std::endl << "--- Example 3: PlotBuilder with Pre-built Zones ---" << std::endl;

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

        std::cout << "✓ Created plot: " << plot1.get_name() << " (" << plot1.get_type() << ")" << std::endl;
        std::cout << "  Total zones: " << plot1.get_zone_count() << std::endl;
        std::cout << "  Owner: " << plot1.get_property("farm_owner") << std::endl;

        for (const auto &zone : plot1.get_zones()) {
            std::cout << "    - " << zone.name() << " (" << zone.type() << ", crop: " << zone.get_property("crop")
                      << ")" << std::endl;
        }
    } catch (const std::exception &e) {
        std::cout << "✗ Failed to build plot: " << e.what() << std::endl;
    }

    // ========================================================================
    // Example 4: PlotBuilder with Inline Zone Construction (Lambda)
    // ========================================================================
    std::cout << std::endl << "--- Example 4: PlotBuilder with Inline Zones ---" << std::endl;

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

        std::cout << "✓ Created plot: " << plot2.get_name() << " (" << plot2.get_type() << ")" << std::endl;
        std::cout << "  Total zones: " << plot2.get_zone_count() << std::endl;
        std::cout << "  Farm type: " << plot2.get_property("farm_type") << std::endl;

        for (const auto &zone : plot2.get_zones()) {
            std::cout << "    - " << zone.name() << " (" << zone.type() << ") - " << zone.raster_info() << std::endl;
        }
    } catch (const std::exception &e) {
        std::cout << "✗ Failed to build plot: " << e.what() << std::endl;
    }

    // ========================================================================
    // Example 5: Complex Multi-Resolution Plot with All Features
    // ========================================================================
    std::cout << std::endl << "--- Example 5: Complex Multi-Resolution Plot ---" << std::endl;

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

        std::cout << "✓ Created complex plot: " << complex_plot.get_name() << " (" << complex_plot.get_type() << ")"
                  << std::endl;
        std::cout << "  Total zones: " << complex_plot.get_zone_count() << std::endl;
        std::cout << "  Owner: " << complex_plot.get_property("owner") << std::endl;
        std::cout << "  Location: " << complex_plot.get_property("location") << std::endl;

        std::cout << std::endl << "  Zone details:" << std::endl;
        for (const auto &zone : complex_plot.get_zones()) {
            std::cout << "    - " << zone.name() << " (" << zone.type() << ")" << std::endl;
            std::cout << "      " << zone.raster_info() << std::endl;
            std::cout << "      " << zone.feature_info() << std::endl;
        }

        // Save the complex plot
        std::filesystem::path save_dir = "complex_builder_plot";
        std::filesystem::remove_all(save_dir);
        complex_plot.save(save_dir);
        std::cout << std::endl << "✓ Saved complex plot to: " << save_dir.string() << std::endl;

    } catch (const std::exception &e) {
        std::cout << "✗ Failed to build complex plot: " << e.what() << std::endl;
    }

    // ========================================================================
    // Example 6: Validation and Error Handling
    // ========================================================================
    std::cout << std::endl << "--- Example 6: Validation and Error Handling ---" << std::endl;

    // Test validation with missing required fields
    zoneout::ZoneBuilder invalid_builder;
    invalid_builder.with_name("incomplete_zone").with_type("test").with_datum(datum);
    // Missing boundary!

    if (!invalid_builder.is_valid()) {
        std::cout << "✗ Builder validation failed (as expected): " << invalid_builder.validation_error() << std::endl;
    }

    // Attempting to build will throw
    try {
        auto invalid_zone = invalid_builder.build();
        std::cout << "Should not reach here!" << std::endl;
    } catch (const std::invalid_argument &e) {
        std::cout << "✓ Build correctly threw exception: " << e.what() << std::endl;
    }

    // ========================================================================
    // Example 7: Builder Reset and Reuse
    // ========================================================================
    std::cout << std::endl << "--- Example 7: Builder Reset and Reuse ---" << std::endl;

    zoneout::ZoneBuilder reusable_builder;

    // Build first zone
    auto zone_v1 = reusable_builder.with_name("zone_v1")
                       .with_type("agricultural")
                       .with_boundary(create_rectangle(50.0, 50.0))
                       .with_datum(datum)
                       .with_resolution(1.0)
                       .build();

    std::cout << "✓ Built zone: " << zone_v1.name() << std::endl;

    // Reset and reuse for different zone
    reusable_builder.reset();

    auto zone_v2 = reusable_builder.with_name("zone_v2")
                       .with_type("pasture")
                       .with_boundary(create_rectangle(75.0, 40.0))
                       .with_datum(datum)
                       .with_resolution(2.0)
                       .with_property("grass", "clover")
                       .build();

    std::cout << "✓ Built zone after reset: " << zone_v2.name() << std::endl;

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << std::endl << "=== Summary ===" << std::endl;
    std::cout << "✓ Demonstrated ZoneBuilder for fluent zone construction" << std::endl;
    std::cout << "✓ Demonstrated PlotBuilder with pre-built and inline zones" << std::endl;
    std::cout << "✓ Showed validation and error handling" << std::endl;
    std::cout << "✓ Demonstrated builder reset and reuse" << std::endl;
    std::cout << "✓ Created complex multi-resolution plots with features" << std::endl;
    std::cout << std::endl << "Builder patterns make code more:" << std::endl;
    std::cout << "  - Readable: Self-documenting method names" << std::endl;
    std::cout << "  - Maintainable: Easy to add/modify options" << std::endl;
    std::cout << "  - Safe: Validation before construction" << std::endl;
    std::cout << "  - Flexible: Optional parameters with defaults" << std::endl;

    return 0;
}
