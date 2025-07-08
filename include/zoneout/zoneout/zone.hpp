#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "concord/concord.hpp"
#include "entropy/generator.hpp"
#include "polygrid.hpp"
#include "utils/time.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    // Modern Zone class using Vector and Raster as primary data storage
    class Zone {
      public:
        // Primary data storage - Poly for boundaries/elements, Grid for raster data
        Poly poly_data_; // Field boundaries + elements (irrigation, crop rows, obstacles, etc.)
        Grid grid_data_; // Multi-layer raster data (elevation, soil, etc.)

      private:
        UUID id_;
        std::string name_;
        std::string type_;

        // Zone metadata
        std::unordered_map<std::string, std::string> properties_;

      public:
        // ========== Constructors ==========
        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Grid<uint8_t> &initial_grid, const concord::Datum &datum)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
              grid_data_(name, type, "default") {
            setDatum(datum);
            // Add the initial grid as the first layer
            grid_data_.addGrid(initial_grid, "base_layer", "terrain");
            syncToPolyGrid();
        }

        // Constructor that generates grid from polygon's axis-aligned bounding box with noise
        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Datum &datum, double resolution = 1.0)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
              grid_data_(name, type, "default") {
            setDatum(datum);

            // Get axis-aligned bounding box from polygon  
            auto aabb = boundary.getAABB();

            // Calculate grid dimensions based on AABB size and resolution
            // Add padding to ensure we fully encompass the polygon bounds
            double padding = resolution * 2.0; // Add 2 cell padding on each side
            double grid_width = aabb.size().x + padding;
            double grid_height = aabb.size().y + padding;
            
            // Standard grid convention: rows = Y dimension, cols = X dimension
            size_t grid_rows = static_cast<size_t>(std::ceil(grid_height / resolution));  // rows = Y dimension
            size_t grid_cols = static_cast<size_t>(std::ceil(grid_width / resolution));   // cols = X dimension

            // Ensure minimum grid size
            grid_rows = std::max(grid_rows, static_cast<size_t>(1));
            grid_cols = std::max(grid_cols, static_cast<size_t>(1));

            // Since Grid constructor uses pose as center, we can directly use AABB center
            // The grid will extend half its size in each direction from this center
            // Use reverse_y=true to match mathematical coordinate system (Y increases upward)
            concord::Pose grid_pose(aabb.center(), concord::Euler{0, 0, 0});
            concord::Grid<uint8_t> generated_grid(grid_rows, grid_cols, resolution, true, grid_pose, true);
            
            // Debug: Check if grid properly encompasses polygon
            auto grid_corners = generated_grid.corners();
            auto polygon_points = boundary.getPoints();
            
            std::cout << "\n=== GRID vs POLYGON BOUNDS DEBUG ===" << std::endl;
            std::cout << "AABB center: (" << aabb.center().x << ", " << aabb.center().y << ", " << aabb.center().z << ")" << std::endl;
            std::cout << "AABB size: (" << aabb.size().x << " x " << aabb.size().y << ")" << std::endl;
            std::cout << "AABB bounds: (" << aabb.min_point.x << ", " << aabb.min_point.y << ") to (" << aabb.max_point.x << ", " << aabb.max_point.y << ")" << std::endl;
            std::cout << "Grid pose center: (" << grid_pose.point.x << ", " << grid_pose.point.y << ", " << grid_pose.point.z << ")" << std::endl;
            std::cout << "Grid dimensions: " << grid_rows << " rows x " << grid_cols << " cols @ " << resolution << " res" << std::endl;
            std::cout << "Grid size with padding: " << grid_width << " x " << grid_height << " (padding: " << padding << ")" << std::endl;
            std::cout << "Actual grid world size: " << (grid_cols * resolution) << " x " << (grid_rows * resolution) << std::endl;
            std::cout << "Grid corners:" << std::endl;
            for (size_t i = 0; i < grid_corners.size(); ++i) {
                const auto& corner = grid_corners[i];
                std::cout << "  Corner " << i << ": (" << corner.x << ", " << corner.y << ", " << corner.z << ")" << std::endl;
            }
            // Calculate actual grid extent from corners
            // Based on output: 0=bottom-left, 1=top-left, 2=top-right, 3=bottom-right
            double corner_width = grid_corners[2].x - grid_corners[0].x;
            double corner_height = grid_corners[1].y - grid_corners[0].y;
            std::cout << "Grid extent from corners: " << corner_width << " x " << corner_height << std::endl;
            
            // Test coordinate mapping accuracy using new Grid methods
            std::cout << "\n=== COORDINATE MAPPING TEST ===" << std::endl;
            auto test_point = polygon_points[0]; // Use first polygon vertex
            std::cout << "Test polygon point: (" << test_point.x << ", " << test_point.y << ")" << std::endl;
            
            // Use Grid's built-in coordinate conversion
            auto [grid_row, grid_col] = generated_grid.world_to_grid(test_point);
            auto mapped_point = generated_grid.grid_to_world(grid_row, grid_col);
            
            std::cout << "Grid cell from world_to_grid: (" << grid_row << ", " << grid_col << ")" << std::endl;
            std::cout << "Grid cell center from grid_to_world: (" << mapped_point.x << ", " << mapped_point.y << ")" << std::endl;
            
            double distance = std::sqrt(std::pow(mapped_point.x - test_point.x, 2) + std::pow(mapped_point.y - test_point.y, 2));
            std::cout << "Distance from polygon point: " << distance << std::endl;
            std::cout << "Coordinate accuracy: " << (distance < resolution ? "GOOD" : "NEEDS_WORK") << std::endl;
            std::cout << "=== END COORDINATE TEST ===\n" << std::endl;
            
            // Find polygon bounds
            double poly_min_x = polygon_points[0].x, poly_max_x = polygon_points[0].x;
            double poly_min_y = polygon_points[0].y, poly_max_y = polygon_points[0].y;
            for (const auto& pt : polygon_points) {
                poly_min_x = std::min(poly_min_x, pt.x);
                poly_max_x = std::max(poly_max_x, pt.x);
                poly_min_y = std::min(poly_min_y, pt.y);
                poly_max_y = std::max(poly_max_y, pt.y);
            }
            
            // Find grid bounds
            double grid_min_x = grid_corners[0].x, grid_max_x = grid_corners[0].x;
            double grid_min_y = grid_corners[0].y, grid_max_y = grid_corners[0].y;
            for (const auto& corner : grid_corners) {
                grid_min_x = std::min(grid_min_x, corner.x);
                grid_max_x = std::max(grid_max_x, corner.x);
                grid_min_y = std::min(grid_min_y, corner.y);
                grid_max_y = std::max(grid_max_y, corner.y);
            }
            
            std::cout << "Polygon bounds: (" << poly_min_x << ", " << poly_min_y << ") to (" << poly_max_x << ", " << poly_max_y << ")" << std::endl;
            std::cout << "Grid bounds: (" << grid_min_x << ", " << grid_min_y << ") to (" << grid_max_x << ", " << grid_max_y << ")" << std::endl;
            
            // Check if polygon exceeds grid bounds
            bool polygon_exceeds_grid = (poly_min_x < grid_min_x) || (poly_max_x > grid_max_x) || 
                                       (poly_min_y < grid_min_y) || (poly_max_y > grid_max_y);
            
            std::cout << "Polygon exceeds grid bounds: " << (polygon_exceeds_grid ? "YES - PROBLEM!" : "NO - OK") << std::endl;
            
            if (polygon_exceeds_grid) {
                std::cout << "PROBLEM DETAILS:" << std::endl;
                if (poly_min_x < grid_min_x) std::cout << "  Polygon extends " << (grid_min_x - poly_min_x) << " units left of grid" << std::endl;
                if (poly_max_x > grid_max_x) std::cout << "  Polygon extends " << (poly_max_x - grid_max_x) << " units right of grid" << std::endl;
                if (poly_min_y < grid_min_y) std::cout << "  Polygon extends " << (grid_min_y - poly_min_y) << " units below grid" << std::endl;
                if (poly_max_y > grid_max_y) std::cout << "  Polygon extends " << (poly_max_y - grid_max_y) << " units above grid" << std::endl;
            }
            std::cout << "=== END BOUNDS DEBUG ===\n" << std::endl;

            // Configure noise generator
            entropy::NoiseGen noise;
            noise.SetNoiseType(entropy::NoiseGen::NoiseType_OpenSimplex2);
            auto sz = std::max(aabb.size().x, aabb.size().y);
            noise.SetFrequency(sz / 300000.0f);
            noise.SetSeed(std::random_device{}());

            // Use the same robust point-in-polygon testing as addRasterLayer
            // Initialize all cells to zero (outside polygon)
            for (size_t r = 0; r < generated_grid.rows(); ++r) {
                for (size_t c = 0; c < generated_grid.cols(); ++c) {
                    // Get the world coordinates of this grid cell center
                    auto cell_center = generated_grid.get_point(r, c);
                    
                    // Test if the cell center is inside the polygon
                    if (boundary.contains(cell_center)) {
                        // Cell is inside polygon - set to white (255)
                        generated_grid.set_value(r, c, 255);
                    } else {
                        // Cell is outside polygon - set to black (0)
                        generated_grid.set_value(r, c, 0);
                    }
                }
            }

            // Add the generated grid as the base layer
            grid_data_.addGrid(generated_grid, "base_layer", "terrain");
            syncToPolyGrid();
        }

        // ========== Basic Properties ==========
        const UUID &getId() const { return id_; }
        const std::string &getName() const { return name_; }
        const std::string &getType() const { return type_; }

        void setName(const std::string &name) {
            name_ = name;
            poly_data_.setName(name);
            grid_data_.setName(name);
        }

        void setType(const std::string &type) {
            type_ = type;
            poly_data_.setType(type);
            grid_data_.setType(type);
        }

        // Note: Geometric operations (area, perimeter, contains) are now available directly on poly_data_
        // Note: Field boundary operations (set_boundary, get_boundary) are now available directly on poly_data_
        // Note: Field elements operations (add_element, get_elements) are now available directly on poly_data_
        // Note: Raster operations are now available directly on grid_data_

        // ========== Zone Properties ==========
        void setProperty(const std::string &key, const std::string &value) { properties_[key] = value; }

        std::string getProperty(const std::string &key, const std::string &default_value = "") const {
            auto it = properties_.find(key);
            return (it != properties_.end()) ? it->second : default_value;
        }

        const std::unordered_map<std::string, std::string> &getProperties() const { return properties_; }

        // ========== Datum Management ==========
        const concord::Datum &getDatum() const { return poly_data_.getDatum(); }

        void setDatum(const concord::Datum &datum) {
            poly_data_.setDatum(datum);
            grid_data_.setDatum(datum);
        }

        // ========== Raster Layer Management ==========
        // Add raster layer with actual grid data
        void addRasterLayer(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                            const std::unordered_map<std::string, std::string> &properties = {},
                            bool poly_cut = false, int layer_index = -1) {
            if (poly_cut && poly_data_.hasFieldBoundary()) {
                // Create a copy of the grid to modify
                auto modified_grid = grid;

                // Get the boundary polygon
                auto boundary = poly_data_.getFieldBoundary();

                // Debug polygon information
                auto polygon_points = boundary.getPoints();
                std::cout << "\n=== POLYGON DEBUG ===" << std::endl;
                std::cout << "Polygon vertices (" << polygon_points.size() << " points):" << std::endl;
                for (size_t i = 0; i < polygon_points.size(); ++i) {
                    const auto& pt = polygon_points[i];
                    std::cout << "  Point " << i << ": (" << pt.x << ", " << pt.y << ", " << pt.z << ")" << std::endl;
                }
                
                auto polygon_aabb = boundary.getAABB();
                std::cout << "Polygon AABB center: (" << polygon_aabb.center().x << ", " 
                         << polygon_aabb.center().y << ", " << polygon_aabb.center().z << ")" << std::endl;
                std::cout << "Polygon AABB size: (" << polygon_aabb.size().x << " x " 
                         << polygon_aabb.size().y << ")" << std::endl;
                std::cout << "Polygon area: " << boundary.area() << std::endl;
                
                // Debug grid information
                std::cout << "\n=== GRID DEBUG ===" << std::endl;
                std::cout << "Grid dimensions: " << modified_grid.rows() << " x " << modified_grid.cols() << std::endl;
                
                auto grid_corners = modified_grid.corners();
                std::cout << "Grid corners:" << std::endl;
                for (size_t i = 0; i < grid_corners.size(); ++i) {
                    const auto& corner = grid_corners[i];
                    std::cout << "  Corner " << i << ": (" << corner.x << ", " << corner.y << ", " << corner.z << ")" << std::endl;
                }
                
                // Sample some grid cell centers
                std::cout << "Sample grid cell centers:" << std::endl;
                auto point_00 = modified_grid.get_point(0, 0);
                std::cout << "  Cell (0,0): (" << point_00.x << ", " << point_00.y << ", " << point_00.z << ")" << std::endl;
                auto point_0end = modified_grid.get_point(0, modified_grid.cols()-1);
                std::cout << "  Cell (0," << (modified_grid.cols()-1) << "): (" << point_0end.x << ", " << point_0end.y << ", " << point_0end.z << ")" << std::endl;
                auto point_end0 = modified_grid.get_point(modified_grid.rows()-1, 0);
                std::cout << "  Cell (" << (modified_grid.rows()-1) << ",0): (" << point_end0.x << ", " << point_end0.y << ", " << point_end0.z << ")" << std::endl;
                auto point_endend = modified_grid.get_point(modified_grid.rows()-1, modified_grid.cols()-1);
                std::cout << "  Cell (" << (modified_grid.rows()-1) << "," << (modified_grid.cols()-1) << "): (" 
                         << point_endend.x << ", " << point_endend.y << ", " << point_endend.z << ")" << std::endl;
                
                // Test center cell
                size_t center_r = modified_grid.rows() / 2;
                size_t center_c = modified_grid.cols() / 2;
                auto center_point = modified_grid.get_point(center_r, center_c);
                std::cout << "  Center cell (" << center_r << "," << center_c << "): (" << center_point.x << ", " << center_point.y << ", " << center_point.z << ")" << std::endl;
                std::cout << "  Center cell in polygon: " << (boundary.contains(center_point) ? "YES" : "NO") << std::endl;
                
                // Use robust point-in-polygon testing for raster-vector overlay
                std::cout << "\n=== POINT-IN-POLYGON TESTING ===" << std::endl;
                size_t cells_inside = 0;
                size_t total_cells = modified_grid.rows() * modified_grid.cols();
                
                for (size_t r = 0; r < modified_grid.rows(); ++r) {
                    for (size_t c = 0; c < modified_grid.cols(); ++c) {
                        // Get the world coordinates of this grid cell center
                        auto cell_center = modified_grid.get_point(r, c);
                        
                        // Test if the cell center is inside the polygon
                        if (boundary.contains(cell_center)) {
                            cells_inside++;
                            // Cell is inside polygon - keep original value
                        } else {
                            // Cell is outside polygon - zero it out
                            modified_grid.set_value(r, c, 0);
                        }
                    }
                }
                
                // Debug output
                std::cout << "Poly cut result: " << cells_inside << "/" << total_cells 
                         << " cells inside polygon (" << (100.0 * cells_inside / total_cells) << "%)" << std::endl;
                std::cout << "=== END DEBUG ===\n" << std::endl;

                grid_data_.addGrid(modified_grid, name, type, properties);
            } else {
                grid_data_.addGrid(grid, name, type, properties);
            }
        }

        // Helper to display raster configuration
        std::string getRasterInfo() const {
            if (grid_data_.gridCount() > 0) {
                const auto &first_layer = grid_data_.getGrid(0);
                return "Raster size: " + std::to_string(first_layer.grid.cols()) + "x" +
                       std::to_string(first_layer.grid.rows()) + " (" + std::to_string(grid_data_.gridCount()) +
                       " layers)";
            }
            return "No raster layers";
        }

        // ========== Validation ==========
        bool is_valid() const { return poly_data_.isValid() && grid_data_.isValid(); }

        // ========== File I/O ==========
        static Zone fromFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) {
            // Use the loadPolyGrid function to load and validate consistency
            auto [poly, grid] = loadPolyGrid(vector_path, raster_path);

            // Extract datum from loaded poly (they should match due to validation in loadPolyGrid)
            auto datum = poly.getDatum();

            // Create a default base grid if no grid exists, or use the first layer
            concord::Grid<uint8_t> base_grid;
            if (grid.hasGrids()) {
                base_grid = grid.getGrid(0).grid;
            } else {
                // Create a minimal default grid
                concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
                base_grid = concord::Grid<uint8_t>(10, 10, 1.0, true, shift);
            }

            // Create a default boundary for the zone
            concord::Polygon default_boundary;
            Zone zone("", "", default_boundary, base_grid, datum);
            zone.poly_data_ = poly;
            // Replace the default grid_data with the loaded one
            zone.grid_data_ = grid;

            // Extract properties from the loaded components
            if (poly.getName().empty() && !grid.getName().empty()) {
                zone.name_ = grid.getName();
            } else {
                zone.name_ = poly.getName();
            }

            if (poly.getType().empty() && !grid.getType().empty()) {
                zone.type_ = grid.getType();
            } else {
                zone.type_ = poly.getType();
            }

            // Use the UUID from poly (they should match due to validation in loadPolyGrid)
            if (!poly.getId().isNull()) {
                zone.id_ = poly.getId();
            } else if (!grid.getId().isNull()) {
                zone.id_ = grid.getId();
            }

            // Load zone properties from poly field properties (those with "prop_" prefix)
            if (std::filesystem::exists(vector_path)) {
                auto field_props = poly.getFieldProperties();
                for (const auto &[key, value] : field_props) {
                    if (key.substr(0, 5) == "prop_") {
                        zone.setProperty(key.substr(5), value);
                    }
                }
            }

            zone.syncToPolyGrid();
            return zone;
        }

        void toFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) const {
            // Ensure internal consistency before saving
            const_cast<Zone *>(this)->syncToPolyGrid();

            // Create copies and add zone-specific properties
            auto poly_copy = poly_data_;
            auto grid_copy = grid_data_;

            // Ensure copies have the correct UUID (in case they got out of sync)
            poly_copy.setId(id_);
            grid_copy.setId(id_);

            // Add zone properties to poly with prefix to avoid conflicts
            for (const auto &[key, value] : properties_) {
                poly_copy.setFieldProperty("prop_" + key, value);
            }

            // Use the savePolyGrid function for consistency validation
            savePolyGrid(poly_copy, grid_copy, vector_path, raster_path);
        }

        // Legacy accessors for compatibility - prefer direct access to poly_data_ and grid_data_
        const geoson::Vector &getVectorData() const { return poly_data_; }
        const geotiv::Raster &getRasterData() const { return grid_data_; }

        geoson::Vector &getVectorData() { return poly_data_; }
        geotiv::Raster &getRasterData() { return grid_data_; }

        // ========== Unified Global Property Access ==========
        // Unified access to global properties - automatically syncs across poly and grid
        std::string getGlobalProperty(const char *global_name) const {
            // First try poly, then grid
            auto field_props = poly_data_.getFieldProperties();
            auto it = field_props.find(global_name);
            if (it != field_props.end()) {
                return it->second;
            }

            if (grid_data_.hasGrids()) {
                auto metadata = grid_data_.getGlobalProperties();
                auto grid_it = metadata.find(global_name);
                if (grid_it != metadata.end()) {
                    return grid_it->second;
                }
            }
            return "";
        }

        // Set global property on both poly and grid for consistency
        void setGlobalProperty(const char *global_name, const std::string &value) {
            poly_data_.setFieldProperty(global_name, value);
            if (grid_data_.hasGrids()) {
                grid_data_.setGlobalProperty(global_name, value);
            }
        }

        // Sync zone properties to both poly and grid using global names
        void syncToPolyGrid() {
            // Set consistent properties across poly and grid
            poly_data_.setName(name_);
            poly_data_.setType(type_);
            poly_data_.setId(id_); // This ensures UUID consistency

            grid_data_.setName(name_);
            grid_data_.setType(type_);
            grid_data_.setId(id_); // This ensures UUID consistency
        }
    };

} // namespace zoneout
