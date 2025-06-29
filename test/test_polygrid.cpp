#include <doctest/doctest.h>
#include <filesystem>

#include "zoneout/zoneout/polygrid.hpp"

TEST_CASE("PolyGrid Basic Construction") {
    SUBCASE("Poly basic construction") {
        zoneout::Poly poly;
        CHECK(!poly.getId().isNull());
        CHECK(poly.getName() == "");
        CHECK(poly.getType() == "other");
        CHECK(!poly.isValid()); // No name or boundary
    }
    
    SUBCASE("Poly construction with name and type") {
        zoneout::Poly poly("Test Field", "agricultural", "crop");
        CHECK(!poly.getId().isNull());
        CHECK(poly.getName() == "Test Field");
        CHECK(poly.getType() == "agricultural");
        CHECK(!poly.isValid()); // No boundary yet
    }
    
    SUBCASE("Grid basic construction") {
        zoneout::Grid grid;
        CHECK(!grid.getId().isNull());
        CHECK(grid.getName() == "");
        CHECK(grid.getType() == "other");
        CHECK(!grid.isValid()); // No name or grids
    }
    
    SUBCASE("Grid construction with name and type") {
        zoneout::Grid grid("Test Raster", "elevation", "dem");
        CHECK(!grid.getId().isNull());
        CHECK(grid.getName() == "Test Raster");
        CHECK(grid.getType() == "elevation");
        CHECK(!grid.isValid()); // No grids yet
    }
}

TEST_CASE("PolyGrid Properties and Operations") {
    SUBCASE("Poly with boundary") {
        std::vector<concord::Point> points = {
            {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}
        };
        concord::Polygon boundary(points);
        
        zoneout::Poly poly("Test Field", "crop", "agricultural", boundary);
        
        CHECK(poly.hasFieldBoundary());
        CHECK(poly.isValid());
        CHECK(poly.area() > 0.0);
        CHECK(poly.perimeter() > 0.0);
        CHECK(poly.contains({5.0, 5.0, 0.0}));
        CHECK(!poly.contains({15.0, 15.0, 0.0}));
    }
    
    SUBCASE("Grid with data") {
        concord::Datum datum{52.0, 4.0, 10.0};
        zoneout::Grid grid("Test Grid", "elevation", "dem", datum);
        
        // Add a grid layer
        grid.addGrid(10, 10, "elevation_layer", "elevation");
        
        CHECK(grid.hasGrids());
        CHECK(grid.isValid());
        CHECK(grid.gridCount() == 1); // Just the elevation grid
    }
    
}

TEST_CASE("PolyGrid Global Properties Sync") {
    SUBCASE("Poly global properties") {
        zoneout::Poly poly("Test Field", "agricultural", "crop");
        
        // Check that global properties are synced
        CHECK(poly.getGlobalProperty("name") == "Test Field");
        CHECK(poly.getGlobalProperty("type") == "agricultural");
        CHECK(poly.getGlobalProperty("uuid") == poly.getId().toString());
        
        // Test property updates
        poly.setName("Updated Field");
        CHECK(poly.getGlobalProperty("name") == "Updated Field");
        
        poly.setType("pasture");
        CHECK(poly.getGlobalProperty("type") == "pasture");
    }
    
    SUBCASE("Grid global properties") {
        zoneout::Grid grid("Test Grid", "elevation", "dem");
        
        // Add a grid so properties can be stored
        grid.addGrid(5, 5, "test_layer", "elevation");
        
        // Check that global properties are synced
        CHECK(grid.getGlobalProperty("name") == "Test Grid");
        CHECK(grid.getGlobalProperty("type") == "elevation");
        CHECK(grid.getGlobalProperty("uuid") == grid.getId().toString());
        
        // Test property updates
        grid.setName("Updated Grid");
        CHECK(grid.getGlobalProperty("name") == "Updated Grid");
        
        grid.setType("terrain");
        CHECK(grid.getGlobalProperty("type") == "terrain");
    }
}

TEST_CASE("PolyGrid File I/O") {
    const std::filesystem::path poly_file = "/tmp/test_poly.geojson";
    const std::filesystem::path grid_file = "/tmp/test_grid.tiff";
    
    SUBCASE("Poly file I/O") {
        // Create poly with boundary
        std::vector<concord::Point> points = {
            {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}
        };
        concord::Polygon boundary(points);
        zoneout::Poly original_poly("Test Field", "agricultural", "crop", boundary);
        
        // Save to file
        original_poly.toFile(poly_file);
        CHECK(std::filesystem::exists(poly_file));
        
        // Load from file
        zoneout::Poly loaded_poly = zoneout::Poly::fromFile(poly_file);
        
        // Verify properties
        CHECK(loaded_poly.getName() == "Test Field");
        CHECK(loaded_poly.getType() == "agricultural");
        CHECK(loaded_poly.getId() == original_poly.getId());
        CHECK(loaded_poly.hasFieldBoundary());
        CHECK(loaded_poly.isValid());
        
        // Cleanup
        std::filesystem::remove(poly_file);
    }
    
    SUBCASE("Grid file I/O") {
        // Create grid with data
        concord::Datum datum{52.0, 4.0, 10.0};
        zoneout::Grid original_grid("Test Grid", "elevation", "dem", datum);
        original_grid.addGrid(5, 5, "test_layer", "elevation");
        
        // Save to file
        original_grid.toFile(grid_file);
        CHECK(std::filesystem::exists(grid_file));
        
        // Load from file
        zoneout::Grid loaded_grid = zoneout::Grid::fromFile(grid_file);
        
        // Verify properties
        CHECK(loaded_grid.getName() == "Test Grid");
        CHECK(loaded_grid.getType() == "elevation");
        CHECK(loaded_grid.getId() == original_grid.getId());
        CHECK(loaded_grid.hasGrids());
        CHECK(loaded_grid.isValid());
        
        // Cleanup
        std::filesystem::remove(grid_file);
    }
}

TEST_CASE("PolyGrid Combined Operations") {
    const std::filesystem::path poly_file = "/tmp/test_combined_poly.geojson";
    const std::filesystem::path grid_file = "/tmp/test_combined_grid.tiff";
    
    SUBCASE("Save and load matching PolyGrid") {
        // Create matching poly and grid
        std::vector<concord::Point> points = {
            {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}
        };
        concord::Polygon boundary(points);
        
        zoneout::Poly poly("Test Zone", "agricultural", "crop", boundary);
        zoneout::Grid grid("Test Zone", "agricultural", "crop");
        grid.addGrid(10, 10, "elevation", "elevation");
        
        // Make them match
        grid.setName(poly.getName());
        grid.setType(poly.getType());
        // Note: IDs are auto-generated and different, so we need to manually sync one
        // In a real scenario, you'd load from existing files or create with same ID
        
        // For this test, let's create new instances with same ID
        zoneout::UUID shared_id = zoneout::generateUUID();
        poly.setId(shared_id);
        grid.setId(shared_id);
        
        // Save using combined function - this will validate consistency
        // For now, save separately since IDs don't match by default
        poly.toFile(poly_file);
        grid.toFile(grid_file);
        
        // Load using combined function
        auto [loaded_poly, loaded_grid] = zoneout::loadPolyGrid(poly_file, grid_file);
        
        CHECK(loaded_poly.getName() == "Test Zone");
        CHECK(loaded_grid.getName() == "Test Zone");
        CHECK(loaded_poly.getType() == "agricultural");
        CHECK(loaded_grid.getType() == "agricultural");
        
        // Cleanup
        std::filesystem::remove(poly_file);
        std::filesystem::remove(grid_file);
    }
}