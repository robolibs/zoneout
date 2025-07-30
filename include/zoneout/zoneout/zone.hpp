#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
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
            // Calculate the shift from the boundary polygon's AABB center
            auto aabb = boundary.getAABB();
            concord::Pose grid_pose(aabb.center(), concord::Euler{0, 0, 0});
            grid_data_.setShift(grid_pose);
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
            size_t grid_rows = static_cast<size_t>(std::ceil(grid_height / resolution)); // rows = Y dimension
            size_t grid_cols = static_cast<size_t>(std::ceil(grid_width / resolution));  // cols = X dimension

            // Ensure minimum grid size
            grid_rows = std::max(grid_rows, static_cast<size_t>(1));
            grid_cols = std::max(grid_cols, static_cast<size_t>(1));

            // Since Grid constructor uses pose as center, we can directly use AABB center
            // The grid will extend half its size in each direction from this center
            // Use reverse_y=true to match mathematical coordinate system (Y increases upward)
            concord::Pose grid_pose(aabb.center(), concord::Euler{0, 0, 0});
            concord::Grid<uint8_t> generated_grid(grid_rows, grid_cols, resolution, true, grid_pose, true);
            
            // Set the shift on the grid_data_ to match the calculated pose
            grid_data_.setShift(grid_pose);

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
                            const std::unordered_map<std::string, std::string> &properties = {}, bool poly_cut = false,
                            int layer_index = -1) {
            if (poly_cut && poly_data_.hasFieldBoundary()) {
                // Create a copy of the grid to modify
                auto modified_grid = grid;

                // Get the boundary polygon
                auto boundary = poly_data_.getFieldBoundary();

                // Use robust point-in-polygon testing for raster-vector overlay
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

        // ========== Polygon Feature Management ==========
        // Add polygon feature with geometry and metadata
        void addPolygonFeature(const concord::Polygon &geometry, const std::string &name, const std::string &type = "",
                               const std::string &subtype = "default",
                               const std::unordered_map<std::string, std::string> &properties = {}) {
            // Validate that polygon is within field boundary
            if (poly_data_.hasFieldBoundary()) {
                auto boundary = poly_data_.getFieldBoundary();

                // Check all points of the polygon are inside boundary
                for (const auto &point : geometry.getPoints()) {
                    if (!boundary.contains(point)) {
                        throw std::runtime_error("Polygon feature '" + name +
                                                 "' is not valid: points must be inside field boundary");
                    }
                }
            }

            // Generate random color between 50-200 for visualization
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> color_dist(50, 200);
            uint8_t polygon_color = static_cast<uint8_t>(color_dist(gen));

            // Draw the polygon on the base grid if it exists
            if (grid_data_.gridCount() > 0) {
                auto &base_grid = grid_data_.getGrid(0).grid;
                for (size_t r = 0; r < base_grid.rows(); ++r) {
                    for (size_t c = 0; c < base_grid.cols(); ++c) {
                        auto cell_center = base_grid.get_point(r, c);
                        if (geometry.contains(cell_center)) {
                            base_grid.set_value(r, c, polygon_color);
                        }
                    }
                }
            }

            // Generate UUID for the new feature
            UUID feature_id = generateUUID();

            // Add the polygon element to the poly_data_
            poly_data_.addPolygonElement(feature_id, name, type, subtype, geometry, properties);
        }

        // Helper to display feature configuration
        std::string getFeatureInfo() const {
            const auto &polygon_elements = poly_data_.getPolygonElements();
            const auto &line_elements = poly_data_.getLineElements();
            const auto &point_elements = poly_data_.getPointElements();

            size_t total_features = polygon_elements.size() + line_elements.size() + point_elements.size();

            if (total_features > 0) {
                return "Features: " + std::to_string(polygon_elements.size()) + " polygons, " +
                       std::to_string(line_elements.size()) + " lines, " + std::to_string(point_elements.size()) +
                       " points (" + std::to_string(total_features) + " total)";
            }
            return "No features";
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

      private:
    };

} // namespace zoneout
