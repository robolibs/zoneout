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
        void setFieldBoundary(const concord::Polygon& boundary) {
            vector_data_.setFieldBoundary(boundary);
            updateModifiedTime();
        }

        const concord::Polygon& getFieldBoundary() const { 
            return vector_data_.getFieldBoundary(); 
        }

        bool hasFieldBoundary() const { 
            return !vector_data_.getFieldBoundary().getPoints().empty(); 
        }

        // Field geometry operations
        double area() const { 
            return hasFieldBoundary() ? vector_data_.getFieldBoundary().area() : 0.0; 
        }
        
        double perimeter() const { 
            return hasFieldBoundary() ? vector_data_.getFieldBoundary().perimeter() : 0.0; 
        }
        
        bool contains(const concord::Point& point) const {
            return hasFieldBoundary() && vector_data_.getFieldBoundary().contains(point);
        }

        // ========== Field Elements (Vector) ==========
        void addFieldElement(const geoson::Geometry& geometry, const std::string& type, 
                           const std::unordered_map<std::string, std::string>& properties = {}) {
            vector_data_.addElement(geometry, type, properties);
            updateModifiedTime();
        }
        
        // Add specific element types
        void addIrrigationLine(const concord::Path& path, const std::unordered_map<std::string, std::string>& properties = {}) {
            addFieldElement(path, "irrigation_line", properties);
        }
        
        void addCropRow(const concord::Path& path, const std::unordered_map<std::string, std::string>& properties = {}) {
            addFieldElement(path, "crop_row", properties);
        }
        
        void addObstacle(const geoson::Geometry& geometry, const std::unordered_map<std::string, std::string>& properties = {}) {
            addFieldElement(geometry, "obstacle", properties);
        }
        
        void addAccessPath(const concord::Path& path, const std::unordered_map<std::string, std::string>& properties = {}) {
            addFieldElement(path, "access_path", properties);
        }

        // Get field elements
        std::vector<geoson::Element> getFieldElements(const std::string& type = "") const {
            if (type.empty()) {
                std::vector<geoson::Element> all_elements;
                for (size_t i = 0; i < vector_data_.elementCount(); ++i) {
                    all_elements.push_back(vector_data_.getElement(i));
                }
                return all_elements;
            }
            return vector_data_.getElementsByType(type);
        }
        
        std::vector<geoson::Element> getIrrigationLines() const { return getFieldElements("irrigation_line"); }
        std::vector<geoson::Element> getCropRows() const { return getFieldElements("crop_row"); }
        std::vector<geoson::Element> getObstacles() const { return getFieldElements("obstacle"); }
        std::vector<geoson::Element> getAccessPaths() const { return getFieldElements("access_path"); }

        // ========== Raster Layers (Raster) ==========
        void addRasterLayer(const std::string& name, const std::string& type,
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
        
        // Add specific layer types
        void addElevationLayer(const concord::Grid<uint8_t>& grid, const std::string& units = "meters") {
            std::unordered_map<std::string, std::string> props;
            props["units"] = units;
            props["datum"] = "sea_level";
            addRasterLayer("elevation", "height", grid, props);
        }
        
        void addSoilMoistureLayer(const concord::Grid<uint8_t>& grid, const std::string& units = "percentage") {
            std::unordered_map<std::string, std::string> props;
            props["units"] = units;
            props["sensor_type"] = "capacitive";
            addRasterLayer("soil_moisture", "moisture", grid, props);
        }
        
        void addCropHealthLayer(const concord::Grid<uint8_t>& grid, const std::string& index_type = "NDVI") {
            std::unordered_map<std::string, std::string> props;
            props["index_type"] = index_type;
            props["range"] = "0-255";
            addRasterLayer("crop_health", "vegetation_index", grid, props);
        }

        // Get raster layers
        std::vector<std::string> getRasterLayerNames() const {
            return raster_data_.getGridNames();
        }
        
        const GridLayer* getRasterLayer(const std::string& name) const {
            auto it = grid_layers_.find(name);
            return (it != grid_layers_.end()) ? &it->second : nullptr;
        }
        
        bool hasRasterLayer(const std::string& name) const {
            return grid_layers_.find(name) != grid_layers_.end();
        }
        
        size_t numRasterLayers() const {
            return grid_layers_.size();
        }

        // Sample raster value at point
        std::optional<uint8_t> sampleRasterAt(const std::string& layer_name, const concord::Point& point) const {
            auto layer = getRasterLayer(layer_name);
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
        bool isValid() const { 
            return hasFieldBoundary() && !name_.empty(); 
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
            }
            
            // Load raster data (multi-layer grids)
            if (std::filesystem::exists(raster_path)) {
                zone.raster_data_ = geotiv::Raster::fromFile(raster_path);
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
            vector_copy.toFile(vector_path);
            
            // Save raster data
            raster_data_.toFile(raster_path);
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