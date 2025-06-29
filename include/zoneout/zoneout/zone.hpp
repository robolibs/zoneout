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

#include "utils/time.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    // Global zone property names for consistent access
    namespace zone_globals {
        static constexpr const char* NAME = "name";
        static constexpr const char* UUID = "uuid";
        static constexpr const char* TYPE = "type";
        static constexpr const char* OWNER_ROBOT_ID = "owner_robot_id";
        static constexpr const char* CREATED_TIME = "created_time";
        static constexpr const char* MODIFIED_TIME = "modified_time";
        static constexpr const char* AREA = "area";
        static constexpr const char* PERIMETER = "perimeter";
        static constexpr const char* IS_VALID = "is_valid";
        static constexpr const char* HAS_BOUNDARY = "has_boundary";
        static constexpr const char* HAS_OWNER = "has_owner";
    }

    // Modern Zone class using Vector and Raster as primary data storage
    class Zone {
      private:
        UUID id_;
        std::string name_;
        std::string type_;

        // Primary data storage - Vector for boundaries/elements, Raster for grid data
        geoson::Vector vector_data_; // Field boundaries + elements (irrigation, crop rows, obstacles, etc.)
        geotiv::Raster raster_data_; // Multi-layer raster data (elevation, soil, etc.)

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
            : id_(generateUUID()), type_("other"), vector_data_(concord::Polygon{}), raster_data_(),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        Zone(const std::string &name, const std::string &type)
            : id_(generateUUID()), name_(name), type_(type), vector_data_(concord::Polygon{}), raster_data_(),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary)
            : id_(generateUUID()), name_(name), type_(type), vector_data_(boundary), raster_data_(),
              created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        // ========== Basic Properties ==========
        const UUID &getId() const { return id_; }
        const std::string &getName() const { return name_; }
        const std::string &getType() const { return type_; }

        void setName(const std::string &name) {
            name_ = name;
            updateModifiedTime();
        }

        void setType(const std::string &type) {
            type_ = type;
            updateModifiedTime();
        }

        // ========== Field Boundary (Vector) ==========
        void set_boundary(const concord::Polygon &boundary) {
            vector_data_.setFieldBoundary(boundary);
            updateModifiedTime();
        }

        const concord::Polygon &get_boundary() const { return vector_data_.getFieldBoundary(); }

        bool has_boundary() const { return !vector_data_.getFieldBoundary().getPoints().empty(); }

        // Field geometry operations
        double area() const { return has_boundary() ? vector_data_.getFieldBoundary().area() : 0.0; }

        double perimeter() const { return has_boundary() ? vector_data_.getFieldBoundary().perimeter() : 0.0; }

        bool contains(const concord::Point &point) const {
            return has_boundary() && vector_data_.getFieldBoundary().contains(point);
        }

        // ========== Field Elements (Vector) ==========
        // Generic method - users name their own element types
        void add_element(const geoson::Geometry &geometry, const std::string &type,
                         const std::unordered_map<std::string, std::string> &properties = {}) {
            vector_data_.addElement(geometry, type, properties);
            updateModifiedTime();
        }

        // Get field elements by type (or all if type is empty)
        std::vector<geoson::Element> get_elements(const std::string &type = "") const {
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
            updateModifiedTime();
        }

        const UUID &getOwnerRobot() const { return owner_robot_id_; }
        bool hasOwner() const { return !owner_robot_id_.isNull(); }

        void releaseOwnership() {
            owner_robot_id_ = UUID::null();
            updateModifiedTime();
        }

        // ========== Timestamps ==========
        const Timestamp &getCreatedTime() const { return created_time_; }
        const Timestamp &getModifiedTime() const { return modified_time_; }

        // ========== Validation ==========
        bool is_valid() const { return has_boundary() && !name_.empty(); }

        // ========== File I/O ==========
        static Zone fromFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) {
            Zone zone;

            std::string vector_name, vector_uuid;
            std::string raster_name, raster_uuid;

            // Load vector data (field boundary + elements)
            if (std::filesystem::exists(vector_path)) {
                zone.vector_data_ = geoson::Vector::fromFile(vector_path);

                // Extract zone metadata from field properties using global names
                auto field_props = zone.vector_data_.getFieldProperties();
                auto name_it = field_props.find(zone_globals::NAME);
                if (name_it != field_props.end()) {
                    vector_name = name_it->second;
                }
                auto type_it = field_props.find(zone_globals::TYPE);
                if (type_it != field_props.end()) {
                    zone.setType(type_it->second);
                }
                auto uuid_it = field_props.find(zone_globals::UUID);
                if (uuid_it != field_props.end()) {
                    vector_uuid = uuid_it->second;
                }
                auto owner_it = field_props.find(zone_globals::OWNER_ROBOT_ID);
                if (owner_it != field_props.end()) {
                    zone.owner_robot_id_ = UUID(owner_it->second);
                }

                // Load zone properties (those with "prop_" prefix)
                for (const auto &[key, value] : field_props) {
                    if (key.substr(0, 5) == "prop_") {
                        zone.setProperty(key.substr(5), value);
                    }
                }
            }

            // Load raster data (multi-layer grids)
            if (std::filesystem::exists(raster_path)) {
                zone.raster_data_ = geotiv::Raster::fromFile(raster_path);
                
                // Extract zone metadata from raster global properties using global names
                auto raster_metadata = zone.raster_data_.getGlobalProperties();
                auto name_it = raster_metadata.find(zone_globals::NAME);
                if (name_it != raster_metadata.end()) {
                    raster_name = name_it->second;
                }
                auto type_it = raster_metadata.find(zone_globals::TYPE);
                if (type_it != raster_metadata.end() && zone.type_.empty()) {
                    zone.setType(type_it->second);
                }
                auto uuid_it = raster_metadata.find(zone_globals::UUID);
                if (uuid_it != raster_metadata.end()) {
                    raster_uuid = uuid_it->second;
                }
                auto owner_it = raster_metadata.find(zone_globals::OWNER_ROBOT_ID);
                if (owner_it != raster_metadata.end() && zone.owner_robot_id_.isNull()) {
                    zone.owner_robot_id_ = UUID(owner_it->second);
                }
            }

            // Validate and resolve UUID conflicts
            if (!vector_uuid.empty() && !raster_uuid.empty()) {
                if (vector_uuid != raster_uuid) {
                    throw std::runtime_error("UUID mismatch between vector (" + vector_uuid + 
                                           ") and raster (" + raster_uuid + ") data files");
                }
                zone.id_ = UUID(vector_uuid);
            } else if (!vector_uuid.empty()) {
                zone.id_ = UUID(vector_uuid);
            } else if (!raster_uuid.empty()) {
                zone.id_ = UUID(raster_uuid);
            }

            // Validate and resolve NAME conflicts
            if (!vector_name.empty() && !raster_name.empty()) {
                if (vector_name != raster_name) {
                    throw std::runtime_error("Name mismatch between vector ('" + vector_name + 
                                           "') and raster ('" + raster_name + "') data files");
                }
                zone.setName(vector_name);
            } else if (!vector_name.empty()) {
                zone.setName(vector_name);
            } else if (!raster_name.empty()) {
                zone.setName(raster_name);
            }

            return zone;
        }

        void toFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) const {
            // Save vector data with zone metadata using global names
            auto vector_copy = vector_data_;
            vector_copy.setFieldProperty(zone_globals::NAME, name_);
            vector_copy.setFieldProperty(zone_globals::TYPE, type_);
            vector_copy.setFieldProperty(zone_globals::UUID, id_.toString());
            vector_copy.setFieldProperty(zone_globals::OWNER_ROBOT_ID, owner_robot_id_.toString());
            vector_copy.setFieldProperty(zone_globals::CREATED_TIME, std::to_string(created_time_.time_since_epoch().count()));
            vector_copy.setFieldProperty(zone_globals::MODIFIED_TIME, std::to_string(modified_time_.time_since_epoch().count()));

            // Save all zone properties with a prefix to avoid conflicts
            for (const auto &[key, value] : properties_) {
                vector_copy.setFieldProperty("prop_" + key, value);
            }

            vector_copy.toFile(vector_path);

            // Save raster data with zone metadata using global names
            if (raster_data_.hasGrids()) {
                auto raster_copy = raster_data_;
                raster_copy.setGlobalProperty(zone_globals::NAME, name_);
                raster_copy.setGlobalProperty(zone_globals::TYPE, type_);
                raster_copy.setGlobalProperty(zone_globals::UUID, id_.toString());
                raster_copy.setGlobalProperty(zone_globals::OWNER_ROBOT_ID, owner_robot_id_.toString());
                raster_copy.setGlobalProperty(zone_globals::CREATED_TIME, std::to_string(created_time_.time_since_epoch().count()));
                raster_copy.setGlobalProperty(zone_globals::MODIFIED_TIME, std::to_string(modified_time_.time_since_epoch().count()));
                
                raster_copy.toFile(raster_path);
            }
        }

        const geoson::Vector &getVectorData() const { return vector_data_; }
        const geotiv::Raster &getRasterData() const { return raster_data_; }

        geoson::Vector &getVectorData() { return vector_data_; }
        geotiv::Raster &getRasterData() { return raster_data_; }

        // ========== Integrated Global Name Access ==========
        // Get zone property from vector field properties using global names
        std::string getVectorProperty(const char* global_name) const {
            auto field_props = vector_data_.getFieldProperties();
            auto it = field_props.find(global_name);
            return (it != field_props.end()) ? it->second : "";
        }

        // Set zone property in vector field properties using global names
        void setVectorProperty(const char* global_name, const std::string& value) {
            vector_data_.setFieldProperty(global_name, value);
            updateModifiedTime();
        }

        // Get zone property from raster global properties using global names
        std::string getRasterProperty(const char* global_name) const {
            auto metadata = raster_data_.getGlobalProperties();
            auto it = metadata.find(global_name);
            return (it != metadata.end()) ? it->second : "";
        }

        // Set zone property in raster global properties using global names
        void setRasterProperty(const char* global_name, const std::string& value) {
            raster_data_.setGlobalProperty(global_name, value);
            updateModifiedTime();
        }

        // Sync zone properties to both vector and raster using global names
        void syncPropertiesToData() {
            setVectorProperty(zone_globals::NAME, name_);
            setVectorProperty(zone_globals::TYPE, type_);
            setVectorProperty(zone_globals::UUID, id_.toString());
            setVectorProperty(zone_globals::OWNER_ROBOT_ID, owner_robot_id_.toString());
            
            if (raster_data_.hasGrids()) {
                setRasterProperty(zone_globals::NAME, name_);
                setRasterProperty(zone_globals::TYPE, type_);
                setRasterProperty(zone_globals::UUID, id_.toString());
                setRasterProperty(zone_globals::OWNER_ROBOT_ID, owner_robot_id_.toString());
            }
        }
    };

} // namespace zoneout
