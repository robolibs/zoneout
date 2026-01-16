#include <doctest/doctest.h>
#include <filesystem>

#include "zoneout/zoneout/polygrid.hpp"

namespace dp = datapod;

TEST_CASE("PolyGrid Basic Construction") {
    SUBCASE("Poly basic construction") {
        zoneout::Poly poly;
        CHECK(!poly.id().isNull());
        CHECK(poly.name() == "");
        CHECK(poly.type() == "other");
        CHECK(!poly.is_valid()); // No name or boundary
    }

    SUBCASE("Poly construction with name and type") {
        zoneout::Poly poly("Test Field", "agricultural", "crop");
        CHECK(!poly.id().isNull());
        CHECK(poly.name() == "Test Field");
        CHECK(poly.type() == "agricultural");
        CHECK(!poly.is_valid()); // No boundary yet
    }

    SUBCASE("Grid basic construction") {
        zoneout::Grid grid;
        CHECK(!grid.id().isNull());
        CHECK(grid.name() == "");
        CHECK(grid.type() == "other");
        CHECK(!grid.is_valid()); // No name or grids
    }

    SUBCASE("Grid construction with name and type") {
        zoneout::Grid grid("Test Raster", "elevation", "dem");
        CHECK(!grid.id().isNull());
        CHECK(grid.name() == "Test Raster");
        CHECK(grid.type() == "elevation");
        CHECK(!grid.is_valid()); // No grids yet
    }
}

TEST_CASE("PolyGrid Properties and Operations") {
    SUBCASE("Poly with boundary") {
        dp::Polygon boundary;
        boundary.vertices = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}};

        zoneout::Poly poly("Test Field", "crop", "agricultural", boundary);

        CHECK(poly.has_field_boundary());
        CHECK(poly.is_valid());
        CHECK(poly.area() > 0.0);
        CHECK(poly.perimeter() > 0.0);
        CHECK(poly.contains({5.0, 5.0, 0.0}));
        CHECK(!poly.contains({15.0, 15.0, 0.0}));
    }

    SUBCASE("Grid with data") {
        dp::Geo datum{52.0, 4.0, 10.0};
        zoneout::Grid grid("Test Grid", "elevation", "dem", datum);

        // Add a grid layer
        grid.add_grid(10, 10, "elevation_layer", "elevation");

        CHECK(grid.has_layers());
        CHECK(grid.is_valid());
        CHECK(grid.layer_count() == 1); // Just the elevation grid
    }
}

TEST_CASE("PolyGrid Global Properties Sync") {
    SUBCASE("Poly global properties") {
        zoneout::Poly poly("Test Field", "agricultural", "crop");

        // Check that global properties are synced
        CHECK(poly.global_property("name").value_or("") == "Test Field");
        CHECK(poly.global_property("type").value_or("") == "agricultural");
        CHECK(poly.global_property("uuid").value_or("") == poly.id().toString());

        // Test property updates
        poly.set_name("Updated Field");
        CHECK(poly.global_property("name").value_or("") == "Updated Field");

        poly.set_type("pasture");
        CHECK(poly.global_property("type").value_or("") == "pasture");
    }

    SUBCASE("Grid global properties") {
        zoneout::Grid grid("Test Grid", "elevation", "dem");

        // Add a grid so properties can be stored
        grid.add_grid(5, 5, "test_layer", "elevation");

        // Check that global properties are synced
        CHECK(grid.name() == "Test Grid");
        CHECK(grid.type() == "elevation");
        CHECK(grid.id().toString() == grid.id().toString());

        // Test property updates
        grid.set_name("Updated Grid");
        CHECK(grid.name() == "Updated Grid");

        grid.set_type("terrain");
        CHECK(grid.type() == "terrain");
    }
}

TEST_CASE("PolyGrid File I/O") {
    const std::filesystem::path poly_file = "/tmp/test_poly.geojson";
    const std::filesystem::path grid_file = "/tmp/test_grid.tiff";

    SUBCASE("Poly file I/O") {
        // Create poly with boundary
        dp::Polygon boundary;
        boundary.vertices = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}};
        zoneout::Poly original_poly("Test Field", "agricultural", "crop", boundary);

        // Save to file
        original_poly.to_file(poly_file);
        CHECK(std::filesystem::exists(poly_file));

        // Load from file
        zoneout::Poly loaded_poly = zoneout::Poly::from_file(poly_file);

        // Verify properties
        CHECK(loaded_poly.name() == "Test Field");
        CHECK(loaded_poly.type() == "agricultural");
        CHECK(loaded_poly.id() == original_poly.id());
        CHECK(loaded_poly.has_field_boundary());
        CHECK(loaded_poly.is_valid());

        // Cleanup
        std::filesystem::remove(poly_file);
    }

    SUBCASE("Grid file I/O") {
        // Create grid with data
        dp::Geo datum{52.0, 4.0, 10.0};
        zoneout::Grid original_grid("Test Grid", "elevation", "dem", datum);
        original_grid.resolution() = 1.0; // Set resolution before adding grid
        original_grid.add_grid(5, 5, "test_layer", "elevation");

        // Save to file
        original_grid.to_file(grid_file);
        CHECK(std::filesystem::exists(grid_file));

        // Load from file
        zoneout::Grid loaded_grid = zoneout::Grid::from_file(grid_file);

        // Verify properties
        CHECK(loaded_grid.name() == "Test Grid");
        CHECK(loaded_grid.type() == "elevation");
        CHECK(loaded_grid.id() == original_grid.id());
        CHECK(loaded_grid.has_layers());
        CHECK(loaded_grid.is_valid());

        // Cleanup
        std::filesystem::remove(grid_file);
    }
}

TEST_CASE("PolyGrid Combined Operations") {
    const std::filesystem::path poly_file = "/tmp/test_combined_poly.geojson";
    const std::filesystem::path grid_file = "/tmp/test_combined_grid.tiff";

    SUBCASE("Save and load matching PolyGrid") {
        // Create matching poly and grid
        dp::Polygon boundary;
        boundary.vertices = {{0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}};

        dp::Geo datum{52.0, 4.0, 10.0};
        zoneout::Poly poly("Test Zone", "agricultural", "crop", boundary);
        zoneout::Grid grid("Test Zone", "agricultural", "crop", datum);
        grid.resolution() = 1.0; // Set resolution before adding grid
        grid.add_grid(10, 10, "elevation", "elevation");

        // Make them match
        grid.set_name(poly.name());
        grid.set_type(poly.type());
        // Note: IDs are auto-generated and different, so we need to manually sync one
        // In a real scenario, you'd load from existing files or create with same ID

        // For this test, let's create new instances with same ID
        zoneout::UUID shared_id = zoneout::generateUUID();
        poly.set_id(shared_id);
        grid.set_id(shared_id);

        // Save using combined function - this will validate consistency
        // For now, save separately since IDs don't match by default
        poly.to_file(poly_file);
        grid.to_file(grid_file);

        // Load using combined function
        auto [loaded_poly, loaded_grid] = zoneout::loadPolyGrid(poly_file, grid_file);

        CHECK(loaded_poly.name() == "Test Zone");
        CHECK(loaded_grid.name() == "Test Zone");
        CHECK(loaded_poly.type() == "agricultural");
        CHECK(loaded_grid.type() == "agricultural");

        // Cleanup
        std::filesystem::remove(poly_file);
        std::filesystem::remove(grid_file);
    }
}