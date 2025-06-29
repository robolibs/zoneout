#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "concord/concord.hpp"
#include "geoson/vector.hpp"
#include "geotiv/raster.hpp"

#include "time_utils.hpp"
#include "uuid_utils.hpp"

namespace zoneout {

    // Modern Zone class using Vector and Raster as primary data storage
    class Zone {
      private:
        // Internal struct for storing grid layers with actual data
        struct GridLayer {
            std::string name;
            std::string type;
            concord::Grid<uint8_t> grid;
            std::unordered_map<std::string, std::string> properties;
        };
        
        UUID id_;
        std::string name_;
        std::string type_;
        
        // Primary data storage - Vector for boundaries/elements, Raster for grid data
        geoson::Vector vector_data_;   // Field boundaries + elements (irrigation, crop rows, obstacles, etc.)
        geotiv::Raster raster_data_;   // Multi-layer raster data (elevation, soil, etc.)
        
        // Actual grid data storage (separate from geotiv metadata)
        std::unordered_map<std::string, GridLayer> grid_layers_;
        
        // Zone metadata
        std::unordered_map<std::string, std::string> properties_;
        UUID owner_robot_id_ = UUID::null();
        Timestamp created_time_;
        Timestamp modified_time_;

      private:
        void updateModifiedTime() { modified_time_ = time_utils::now(); }

      public:
        // ========== Constructors ==========
        Zone() : id_(generateUUID()), type_("other"), 
                vector_data_(concord::Polygon{}), raster_data_(),
                created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        Zone(const std::string &name, const std::string &type)
            : id_(generateUUID()), name_(name), type_(type),
              vector_data_(concord::Polygon{}), raster_data_(),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary)
            : id_(generateUUID()), name_(name), type_(type),
              vector_data_(boundary), raster_data_(),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        // ========== Basic Properties ==========
        const UUID& getId() const { return id_; }
        const std::string& getName() const { return name_; }
        const std::string& getType() const { return type_; }
        
        void setName(const std::string& name) { 
            name_ = name; 
            updateModifiedTime(); 
        }
        
        void setType(const std::string& type) { 
            type_ = type; 
            updateModifiedTime(); 
        }

        // ========== Field Boundary (Vector) ==========
        void set_boundary(const concord::Polygon& boundary) {
            vector_data_.setFieldBoundary(boundary);
            updateModifiedTime();
        }

        const concord::Polygon& get_boundary() const { 
            return vector_data_.getFieldBoundary(); 
        }

        bool has_boundary() const { 
            return !vector_data_.getFieldBoundary().getPoints().empty(); 
        }

        // Field geometry operations
        double area() const { 
            return has_boundary() ? vector_data_.getFieldBoundary().area() : 0.0; 
        }
        
        double perimeter() const { 
            return has_boundary() ? vector_data_.getFieldBoundary().perimeter() : 0.0; 
        }
        
        bool contains(const concord::Point& point) const {
            return has_boundary() && vector_data_.getFieldBoundary().contains(point);
        }

        // ========== Field Elements (Vector) ==========
        // Generic method - users name their own element types
        void add_element(const geoson::Geometry& geometry, const std::string& type, 
                        const std::unordered_map<std::string, std::string>& properties = {}) {
            vector_data_.addElement(geometry, type, properties);
            updateModifiedTime();
        }

        // Get field elements by type (or all if type is empty)
        std::vector<geoson::Element> get_elements(const std::string& type = "") const {
            if (type.empty()) {
                std::vector<geoson::Element> all_elements;
                for (size_t i = 0; i < vector_data_.elementCount(); ++i) {
                    all_elements.push_back(vector_data_.getElement(i));
                }
                return all_elements;
            }
            return vector_data_.getElementsByType(type);
        }

        // ========== Raster Layers (Raster) ==========
        // Generic method - users name their own layer types
        void add_layer(const std::string& name, const std::string& type,
                      const concord::Grid<uint8_t>& grid,
                      const std::unordered_map<std::string, std::string>& properties = {}) {
            // Store grid metadata in geotiv raster
            raster_data_.addGrid(grid.cols(), grid.rows(), name, type, properties);
            
            // Store the actual grid data separately for sampling
            GridLayer layer;
            layer.name = name;
            layer.type = type;
            layer.grid = grid;
            layer.properties = properties;
            grid_layers_[name] = layer;
            
            updateModifiedTime();
        }

        // Get raster layer information
        std::vector<std::string> get_layer_names() const {
            std::vector<std::string> names;
            for (const auto& [name, layer] : grid_layers_) {
                names.push_back(name);
            }
            return names;
        }
        
        const GridLayer* get_layer(const std::string& name) const {
            auto it = grid_layers_.find(name);
            return (it != grid_layers_.end()) ? &it->second : nullptr;
        }
        
        bool has_layer(const std::string& name) const {
            return grid_layers_.find(name) != grid_layers_.end();
        }
        
        size_t num_layers() const {
            return grid_layers_.size();
        }

        // Sample raster value at point
        std::optional<uint8_t> sample_at(const std::string& layer_name, const concord::Point& point) const {
            auto layer = get_layer(layer_name);
            if (!layer) return std::nullopt;
            
            // Simple point-in-grid sampling (could be enhanced with interpolation)
            auto corners = layer->grid.corners();
            auto min_corner = corners[0];
            
            // Use the grid's actual resolution (inradius) instead of raster resolution
            double grid_resolution = layer->grid.inradius();
            double dx = (point.x - min_corner.x) / grid_resolution;
            double dy = (point.y - min_corner.y) / grid_resolution;
            
            size_t row = static_cast<size_t>(std::max(0.0, std::min(dy, static_cast<double>(layer->grid.rows() - 1))));
            size_t col = static_cast<size_t>(std::max(0.0, std::min(dx, static_cast<double>(layer->grid.cols() - 1))));
            
            return layer->grid(row, col).second;
        }

        // ========== Zone Properties ==========
        void setProperty(const std::string& key, const std::string& value) {
            properties_[key] = value;
            updateModifiedTime();
        }
        
        std::string getProperty(const std::string& key, const std::string& default_value = "") const {
            auto it = properties_.find(key);
            return (it != properties_.end()) ? it->second : default_value;
        }
        
        const std::unordered_map<std::string, std::string>& getProperties() const {
            return properties_;
        }

        // ========== Robot Ownership ==========
        void setOwnerRobot(const UUID& robot_id) {
            owner_robot_id_ = robot_id;
            updateModifiedTime();
        }
        
        const UUID& getOwnerRobot() const { return owner_robot_id_; }
        bool hasOwner() const { return !owner_robot_id_.isNull(); }
        
        void releaseOwnership() {
            owner_robot_id_ = UUID::null();
            updateModifiedTime();
        }

        // ========== Timestamps ==========
        const Timestamp& getCreatedTime() const { return created_time_; }
        const Timestamp& getModifiedTime() const { return modified_time_; }

        // ========== Validation ==========
        bool is_valid() const { 
            return has_boundary() && !name_.empty(); 
        }

        // ========== File I/O ==========
        static Zone fromFiles(const std::filesystem::path& vector_path,
                            const std::filesystem::path& raster_path) {
            Zone zone;
            
            // Load vector data (field boundary + elements)
            if (std::filesystem::exists(vector_path)) {
                zone.vector_data_ = geoson::Vector::fromFile(vector_path);
                
                // Extract zone metadata from field properties
                auto field_props = zone.vector_data_.getFieldProperties();
                auto name_it = field_props.find("name");
                if (name_it != field_props.end()) {
                    zone.setName(name_it->second);
                }
                auto type_it = field_props.find("type");
                if (type_it != field_props.end()) {
                    zone.setType(type_it->second);
                }
                
                // Load zone properties (those with "prop_" prefix)
                for (const auto& [key, value] : field_props) {
                    if (key.substr(0, 5) == "prop_") {
                        zone.setProperty(key.substr(5), value);
                    }
                }
            }
            
            // Load raster data (multi-layer grids)
            if (std::filesystem::exists(raster_path)) {
                zone.raster_data_ = geotiv::Raster::fromFile(raster_path);
                
                // Convert geotiv raster layers back to our grid layers
                // Don't use add_layer as it would duplicate the raster data
                for (size_t i = 0; i < zone.raster_data_.gridCount(); ++i) {
                    const auto& geotiv_layer = zone.raster_data_.getGrid(i);
                    
                    // Create our GridLayer directly
                    GridLayer layer;
                    layer.name = geotiv_layer.name;
                    layer.type = geotiv_layer.type;
                    layer.grid = geotiv_layer.grid;  // Copy the loaded grid
                    layer.properties = geotiv_layer.properties;
                    
                    zone.grid_layers_[layer.name] = layer;
                }
            }
            
            return zone;
        }
        
        void toFiles(const std::filesystem::path& vector_path,
                    const std::filesystem::path& raster_path) const {
            // Save vector data with zone metadata
            auto vector_copy = vector_data_;
            vector_copy.setFieldProperty("name", name_);
            vector_copy.setFieldProperty("type", type_);
            vector_copy.setFieldProperty("id", id_.toString());
            
            // Save all zone properties with a prefix to avoid conflicts
            for (const auto& [key, value] : properties_) {
                vector_copy.setFieldProperty("prop_" + key, value);
            }
            
            vector_copy.toFile(vector_path);
            
            // Save raster data - only if we have actual grid layers
            if (!grid_layers_.empty()) {
                // Create a geotiv::Raster with our actual grid data
                geotiv::Raster raster_to_save;
                
                // Add each grid layer to the raster
                for (const auto& [layer_name, grid_layer] : grid_layers_) {
                    raster_to_save.addGrid(grid_layer.grid.cols(), grid_layer.grid.rows(), 
                                         layer_name, grid_layer.type, grid_layer.properties);
                    
                    // Now get the added grid and copy our actual data to it
                    auto& geotiv_grid = raster_to_save.getGrid(layer_name).grid;
                    for (size_t r = 0; r < grid_layer.grid.rows(); ++r) {
                        for (size_t c = 0; c < grid_layer.grid.cols(); ++c) {
                            auto cell = grid_layer.grid(r, c);
                            geotiv_grid.set_value(r, c, cell.second);
                        }
                    }
                }
                
                raster_to_save.toFile(raster_path);
            }
        }

        // ========== Factory Methods ==========
        static Zone createField(const std::string& name, const concord::Polygon& boundary) {
            Zone field(name, "field", boundary);
            field.setProperty("crop_type", "unknown");
            field.setProperty("planting_date", "");
            field.setProperty("harvest_date", "");
            return field;
        }

        static Zone createBarn(const std::string& name, const concord::Polygon& boundary) {
            Zone barn(name, "barn", boundary);
            barn.setProperty("animal_type", "unknown");
            barn.setProperty("capacity", "0");
            barn.setProperty("ventilation", "natural");
            return barn;
        }

        static Zone createGreenhouse(const std::string& name, const concord::Polygon& boundary) {
            Zone greenhouse(name, "greenhouse", boundary);
            greenhouse.setProperty("climate_control", "automated");
            greenhouse.setProperty("crop_type", "unknown");
            greenhouse.setProperty("temperature_range", "18-25C");
            return greenhouse;
        }

        // ========== Direct Access to Vector/Raster ==========
        const geoson::Vector& getVectorData() const { return vector_data_; }
        const geotiv::Raster& getRasterData() const { return raster_data_; }
        
        geoson::Vector& getVectorData() { return vector_data_; }
        geotiv::Raster& getRasterData() { return raster_data_; }
    };

} // namespace zoneout