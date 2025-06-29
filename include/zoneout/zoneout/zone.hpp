#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "concord/concord.hpp"
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
        concord::Size raster_size_; // Uniform size for all raster layers

        // Zone metadata
        std::unordered_map<std::string, std::string> properties_;

      public:
        // ========== Constructors ==========
        Zone(const concord::Datum &datum, const concord::Size &raster_size = concord::Size{100, 100, 0}) 
            : id_(generateUUID()), type_("other"), raster_size_(raster_size), poly_data_(), grid_data_() { 
            setDatum(datum);
            syncToPolyGrid(); 
        }

        Zone(const std::string &name, const std::string &type, const concord::Datum &datum, const concord::Size &raster_size = concord::Size{100, 100, 0})
            : id_(generateUUID()), name_(name), type_(type), raster_size_(raster_size), poly_data_(name, type, "default"),
              grid_data_(name, type, "default") {
            setDatum(datum);
            syncToPolyGrid();
        }

        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary, const concord::Datum &datum, const concord::Size &raster_size = concord::Size{100, 100, 0})
            : id_(generateUUID()), name_(name), type_(type), raster_size_(raster_size), poly_data_(name, type, "default", boundary),
              grid_data_(name, type, "default") {
            setDatum(datum);
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
        const concord::Datum& getDatum() const { return poly_data_.getDatum(); }
        
        void setDatum(const concord::Datum& datum) {
            poly_data_.setDatum(datum);
            grid_data_.setDatum(datum);
        }

        // ========== Raster Size Management ==========
        const concord::Size& getRasterSize() const { return raster_size_; }
        
        void setRasterSize(const concord::Size& size) { 
            raster_size_ = size; 
        }

        // ========== Raster Layer Management ==========
        void addRasterLayer(const std::string& name, const std::string& type = "", 
                           const std::unordered_map<std::string, std::string>& properties = {}) {
            grid_data_.addGrid(static_cast<uint32_t>(raster_size_.x), 
                              static_cast<uint32_t>(raster_size_.y), 
                              name, type, properties);
        }

        // Add raster layer with actual grid data
        void addRasterLayer(const concord::Grid<uint8_t>& grid, const std::string& name, const std::string& type = "", 
                           const std::unordered_map<std::string, std::string>& properties = {}) {
            // Validate that the grid dimensions match the Zone's raster size
            if (static_cast<double>(grid.cols()) != raster_size_.x || 
                static_cast<double>(grid.rows()) != raster_size_.y) {
                throw std::invalid_argument("Grid dimensions (" + 
                    std::to_string(grid.cols()) + "x" + std::to_string(grid.rows()) + 
                    ") do not match Zone raster size (" + 
                    std::to_string(static_cast<int>(raster_size_.x)) + "x" + 
                    std::to_string(static_cast<int>(raster_size_.y)) + ")");
            }
            grid_data_.addGrid(grid, name, type, properties);
        }

        // Helper to create a grid with the Zone's raster size and proper spatial positioning
        concord::Grid<uint8_t> createGrid() const {
            concord::Point p0;
            concord::Pose shift;
            
            // Use the same logic as geotiv::Raster for spatial positioning
            auto datum = getDatum();
            auto heading = grid_data_.getHeading();
            auto crs = grid_data_.getCRS();
            auto resolution = grid_data_.getResolution();
            
            if (crs == geotiv::CRS::WGS) {
                concord::WGS w0{datum.lat, datum.lon, datum.alt};
                concord::ENU enu0 = w0.toENU(datum);
                p0 = concord::Point{enu0.x, enu0.y, enu0.z};
                shift = concord::Pose{p0, heading};
            } else {
                p0 = concord::Point{0.0, 0.0, 0.0};
                shift = concord::Pose{p0, heading};
            }
            
            return concord::Grid<uint8_t>(static_cast<size_t>(raster_size_.y), 
                                         static_cast<size_t>(raster_size_.x), 
                                         resolution, true, shift);
        }

        // Helper to display raster configuration
        std::string getRasterInfo() const {
            return "Raster size: " + std::to_string(static_cast<int>(raster_size_.x)) + "x" + 
                   std::to_string(static_cast<int>(raster_size_.y)) + 
                   " (" + std::to_string(grid_data_.gridCount()) + " layers)";
        }

        // ========== Validation ==========
        bool is_valid() const { return poly_data_.isValid() && (grid_data_.hasGrids() ? grid_data_.isValid() : true); }

        // ========== File I/O ==========
        static Zone fromFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) {
            // Use the loadPolyGrid function to load and validate consistency
            auto [poly, grid] = loadPolyGrid(vector_path, raster_path);

            // Extract datum from loaded poly (they should match due to validation in loadPolyGrid)
            auto datum = poly.getDatum();
            
            // Extract raster size from loaded grid (if it has layers)
            concord::Size raster_size{100, 100, 0}; // default
            if (grid.hasGrids()) {
                const auto& first_layer = grid.getGrid(0);
                raster_size = concord::Size{static_cast<double>(first_layer.grid.cols()), 
                                          static_cast<double>(first_layer.grid.rows()), 0};
            }
            
            Zone zone(datum, raster_size);
            zone.poly_data_ = poly;
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
