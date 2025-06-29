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

    // Global zone property names for consistent access
    namespace zone_globals {
        static constexpr const char *NAME = "name";
        static constexpr const char *UUID = "uuid";
        static constexpr const char *TYPE = "type";
        static constexpr const char *OWNER_ROBOT_ID = "owner_robot_id";
        static constexpr const char *CREATED_TIME = "created_time";
        static constexpr const char *MODIFIED_TIME = "modified_time";
        static constexpr const char *AREA = "area";
        static constexpr const char *PERIMETER = "perimeter";
        static constexpr const char *IS_VALID = "is_valid";
        static constexpr const char *HAS_BOUNDARY = "has_boundary";
        static constexpr const char *HAS_OWNER = "has_owner";
    } // namespace zone_globals

    // Modern Zone class using Vector and Raster as primary data storage
    class Zone {
      private:
        UUID id_;
        std::string name_;
        std::string type_;

        // Primary data storage - Poly for boundaries/elements, Grid for raster data
        Poly poly_data_; // Field boundaries + elements (irrigation, crop rows, obstacles, etc.)
        Grid grid_data_; // Multi-layer raster data (elevation, soil, etc.)

        // Zone metadata
        std::unordered_map<std::string, std::string> properties_;
        UUID owner_robot_id_ = UUID::null();
        Timestamp created_time_;
        Timestamp modified_time_;

      private:
        void updateModifiedTime() { modified_time_ = time_utils::now(); }

      public:
        // ========== Constructors ==========
        Zone()
            : id_(generateUUID()), type_("other"), poly_data_(), grid_data_(), created_time_(time_utils::now()),
              modified_time_(time_utils::now()) {
            syncToPolyGrid();
        }

        Zone(const std::string &name, const std::string &type)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default"), grid_data_(name, type, "default"),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {
            syncToPolyGrid();
        }

        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary), grid_data_(name, type, "default"),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {
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
            updateModifiedTime();
        }

        void setType(const std::string &type) {
            type_ = type;
            poly_data_.setType(type);
            grid_data_.setType(type);
            updateModifiedTime();
        }

        // ========== Field Boundary (Poly) ==========
        void set_boundary(const concord::Polygon &boundary) {
            poly_data_.setFieldBoundary(boundary);
            updateModifiedTime();
        }

        const concord::Polygon &get_boundary() const { return poly_data_.getFieldBoundary(); }

        bool has_boundary() const { return poly_data_.hasFieldBoundary(); }

        // Field geometry operations
        double area() const { return poly_data_.area(); }

        double perimeter() const { return poly_data_.perimeter(); }

        bool contains(const concord::Point &point) const { return poly_data_.contains(point); }

        // ========== Field Elements (Poly) ==========
        // Generic method - users name their own element types
        void add_element(const geoson::Geometry &geometry, const std::string &type,
                         const std::unordered_map<std::string, std::string> &properties = {}) {
            poly_data_.addElement(geometry, type, properties);
            updateModifiedTime();
        }

        // Get field elements by type (or all if type is empty)
        std::vector<geoson::Element> get_elements(const std::string &type = "") const {
            if (type.empty()) {
                std::vector<geoson::Element> all_elements;
                for (size_t i = 0; i < poly_data_.elementCount(); ++i) {
                    all_elements.push_back(poly_data_.getElement(i));
                }
                return all_elements;
            }
            return poly_data_.getElementsByType(type);
        }

        // ========== Raster Layers (Raster) ==========
        // Direct access to geotiv::Raster - no wrapper methods needed
        // Use getRasterData() to access all raster functionality

        // ========== Zone Properties ==========
        void setProperty(const std::string &key, const std::string &value) {
            properties_[key] = value;
            updateModifiedTime();
        }

        std::string getProperty(const std::string &key, const std::string &default_value = "") const {
            auto it = properties_.find(key);
            return (it != properties_.end()) ? it->second : default_value;
        }

        const std::unordered_map<std::string, std::string> &getProperties() const { return properties_; }

        // ========== Robot Ownership ==========
        void setOwnerRobot(const UUID &robot_id) {
            owner_robot_id_ = robot_id;
            poly_data_.setOwnerRobot(robot_id);
            grid_data_.setOwnerRobot(robot_id);
            updateModifiedTime();
        }

        const UUID &getOwnerRobot() const { return owner_robot_id_; }
        bool hasOwner() const { return !owner_robot_id_.isNull(); }

        void releaseOwnership() {
            owner_robot_id_ = UUID::null();
            poly_data_.releaseOwnership();
            grid_data_.releaseOwnership();
            updateModifiedTime();
        }

        // ========== Timestamps ==========
        const Timestamp &getCreatedTime() const { return created_time_; }
        const Timestamp &getModifiedTime() const { return modified_time_; }

        // ========== Validation ==========
        bool is_valid() const { return poly_data_.isValid(); }

        // ========== File I/O ==========
        static Zone fromFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) {
            // Use the loadPolyGrid function to load and validate consistency
            auto [poly, grid] = loadPolyGrid(vector_path, raster_path);

            Zone zone;
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

            // Use owner from poly (they should match)
            if (poly.hasOwner()) {
                zone.owner_robot_id_ = poly.getOwnerRobot();
            } else if (grid.hasOwner()) {
                zone.owner_robot_id_ = grid.getOwnerRobot();
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

            // Add timestamps to both
            poly_copy.setGlobalProperty(zone_globals::CREATED_TIME,
                                        std::to_string(created_time_.time_since_epoch().count()));
            poly_copy.setGlobalProperty(zone_globals::MODIFIED_TIME,
                                        std::to_string(modified_time_.time_since_epoch().count()));

            if (grid_copy.hasGrids()) {
                grid_copy.setGlobalProperty(zone_globals::CREATED_TIME,
                                            std::to_string(created_time_.time_since_epoch().count()));
                grid_copy.setGlobalProperty(zone_globals::MODIFIED_TIME,
                                            std::to_string(modified_time_.time_since_epoch().count()));
            }

            // Use the savePolyGrid function for consistency validation
            savePolyGrid(poly_copy, grid_copy, vector_path, raster_path);
        }

        const Poly &getPolyData() const { return poly_data_; }
        const Grid &getGridData() const { return grid_data_; }

        Poly &getPolyData() { return poly_data_; }
        Grid &getGridData() { return grid_data_; }

        // Legacy accessors for compatibility
        const geoson::Vector &getVectorData() const { return poly_data_; }
        const geotiv::Raster &getRasterData() const { return grid_data_; }

        geoson::Vector &getVectorData() { return poly_data_; }
        geotiv::Raster &getRasterData() { return grid_data_; }

        // ========== Integrated Global Name Access ==========
        // Get zone property from poly field properties using global names
        std::string getVectorProperty(const char *global_name) const {
            auto field_props = poly_data_.getFieldProperties();
            auto it = field_props.find(global_name);
            return (it != field_props.end()) ? it->second : "";
        }

        // Set zone property in poly field properties using global names
        void setVectorProperty(const char *global_name, const std::string &value) {
            poly_data_.setFieldProperty(global_name, value);
            updateModifiedTime();
        }

        // Get zone property from grid global properties using global names
        std::string getRasterProperty(const char *global_name) const {
            auto metadata = grid_data_.getGlobalProperties();
            auto it = metadata.find(global_name);
            return (it != metadata.end()) ? it->second : "";
        }

        // Set zone property in grid global properties using global names
        void setRasterProperty(const char *global_name, const std::string &value) {
            grid_data_.setGlobalProperty(global_name, value);
            updateModifiedTime();
        }

        // Sync zone properties to both poly and grid using global names
        void syncToPolyGrid() {
            // Set consistent properties across poly and grid
            poly_data_.setName(name_);
            poly_data_.setType(type_);
            poly_data_.setId(id_); // This ensures UUID consistency
            poly_data_.setOwnerRobot(owner_robot_id_);

            grid_data_.setName(name_);
            grid_data_.setType(type_);
            grid_data_.setId(id_); // This ensures UUID consistency
            grid_data_.setOwnerRobot(owner_robot_id_);
        }
    };

} // namespace zoneout
