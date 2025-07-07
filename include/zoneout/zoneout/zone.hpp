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

        // Constructor that generates grid from polygon's oriented bounding box with noise
        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Datum &datum, double resolution = 1.0)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
              grid_data_(name, type, "default") {
            setDatum(datum);

            // Get oriented bounding box from polygon
            auto obb = boundary.get_obb();

            // Calculate grid dimensions based on OBB size and resolution
            size_t grid_rows = static_cast<size_t>(std::ceil(obb.size.x / resolution));
            size_t grid_cols = static_cast<size_t>(std::ceil(obb.size.y / resolution));

            // Ensure minimum grid size
            grid_rows = std::max(grid_rows, static_cast<size_t>(1));
            grid_cols = std::max(grid_cols, static_cast<size_t>(1));

            // Create grid using the OBB pose (center and orientation)
            concord::Grid<uint8_t> generated_grid(grid_rows, grid_cols, resolution, true, obb.pose);

            // Configure noise generator
            entropy::NoiseGen noise;
            noise.SetNoiseType(entropy::NoiseGen::NoiseType_OpenSimplex2);
            auto sz = std::max(obb.size.x, obb.size.y);
            noise.SetFrequency(sz / 300000.0f);
            noise.SetSeed(std::random_device{}());

            // Get indices of cells that are within the polygon
            auto indices_in_polygon = generated_grid.indices_within(boundary);

            // Initialize all cells to zero (outside polygon)
            for (size_t r = 0; r < generated_grid.rows(); ++r) {
                for (size_t c = 0; c < generated_grid.cols(); ++c) {
                    generated_grid.set_value(r, c, 0);
                }
            }

            // Set cells inside polygon to 255 (full white/value)
            for (auto idx : indices_in_polygon) {
                size_t r = idx / generated_grid.cols();
                size_t c = idx % generated_grid.cols();
                generated_grid.set_value(r, c, 255);
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
                
                auto polygon_obb = boundary.get_obb();
                std::cout << "Polygon OBB center: (" << polygon_obb.pose.point.x << ", " 
                         << polygon_obb.pose.point.y << ", " << polygon_obb.pose.point.z << ")" << std::endl;
                std::cout << "Polygon OBB size: (" << polygon_obb.size.x << " x " 
                         << polygon_obb.size.y << ")" << std::endl;
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
