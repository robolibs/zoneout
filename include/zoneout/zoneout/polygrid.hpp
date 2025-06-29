#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "geoson/vector.hpp"
#include "geotiv/raster.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    // Global property names for consistent access across Poly and Grid
    namespace polygrid_globals {
        static constexpr const char *NAME = "name";
        static constexpr const char *UUID = "uuid";
        static constexpr const char *TYPE = "type";
        static constexpr const char *SUBTYPE = "subtype";
        static constexpr const char *OWNER_ROBOT_ID = "owner_robot_id";
        static constexpr const char *CREATED_TIME = "created_time";
        static constexpr const char *MODIFIED_TIME = "modified_time";
        static constexpr const char *AREA = "area";
        static constexpr const char *PERIMETER = "perimeter";
        static constexpr const char *IS_VALID = "is_valid";
        static constexpr const char *HAS_BOUNDARY = "has_boundary";
        static constexpr const char *HAS_OWNER = "has_owner";
    } // namespace polygrid_globals

    // Structured Element types with required metadata
    struct StructuredElement {
        UUID uuid;
        std::string name;
        std::string type;
        std::string subtype;
        std::unordered_map<std::string, std::string> properties;

        StructuredElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                          const std::unordered_map<std::string, std::string> &props = {})
            : uuid(id), name(n), type(t), subtype(st), properties(props) {}

        // Check if element has all required fields
        static bool isValid(const geoson::Element &element) {
            const auto &props = element.properties;
            return props.find(polygrid_globals::UUID) != props.end() &&
                   props.find(polygrid_globals::NAME) != props.end() &&
                   props.find(polygrid_globals::TYPE) != props.end() &&
                   props.find(polygrid_globals::SUBTYPE) != props.end();
        }

        // Create from geoson::Element if valid
        static std::optional<StructuredElement> fromElement(const geoson::Element &element) {
            if (!isValid(element))
                return std::nullopt;

            const auto &props = element.properties;
            return StructuredElement(UUID(props.at(polygrid_globals::UUID)), props.at(polygrid_globals::NAME),
                                     props.at(polygrid_globals::TYPE), props.at(polygrid_globals::SUBTYPE), props);
        }

        // Convert to properties map for storage
        std::unordered_map<std::string, std::string> toProperties() const {
            auto props = properties;
            props[polygrid_globals::UUID] = uuid.toString();
            props[polygrid_globals::NAME] = name;
            props[polygrid_globals::TYPE] = type;
            props[polygrid_globals::SUBTYPE] = subtype;
            return props;
        }
    };

    // Typed element collections
    struct PolygonElement : public StructuredElement {
        concord::Polygon geometry;

        PolygonElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                       const concord::Polygon &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    struct LineElement : public StructuredElement {
        concord::Line geometry;

        LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                    const concord::Line &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    struct PointElement : public StructuredElement {
        concord::Point geometry;

        PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                     const concord::Point &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    // Enhanced Poly class - High-level Vector with structured elements
    class Poly : public geoson::Vector {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::string subtype_;
        UUID owner_robot_id_ = UUID::null();

        // Structured element collections
        std::vector<PolygonElement> polygon_elements_;
        std::vector<LineElement> line_elements_;
        std::vector<PointElement> point_elements_;

      public:
        // ========== Constructors ==========
        Poly() : geoson::Vector(concord::Polygon{}), id_(generateUUID()), type_("other"), subtype_("default") {
            syncToGlobalProperties();
        }

        Poly(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : geoson::Vector(concord::Polygon{}), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
            syncToGlobalProperties();
            loadStructuredElements();
        }

        Poly(const std::string &name, const std::string &type, const std::string &subtype,
             const concord::Polygon &boundary)
            : geoson::Vector(boundary), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
            syncToGlobalProperties();
            loadStructuredElements();
        }

        Poly(const std::string &name, const std::string &type, const std::string &subtype,
             const concord::Polygon &boundary, const concord::Datum &datum, const concord::Euler &heading,
             geoson::CRS crs)
            : geoson::Vector(boundary, datum, heading, crs), id_(generateUUID()), name_(name), type_(type),
              subtype_(subtype) {
            syncToGlobalProperties();
            loadStructuredElements();
        }

        // ========== Basic Properties ==========
        const UUID &getId() const { return id_; }
        const std::string &getName() const { return name_; }
        const std::string &getType() const { return type_; }
        const std::string &getSubtype() const { return subtype_; }

        void setName(const std::string &name) {
            name_ = name;
            syncToGlobalProperties();
        }

        void setType(const std::string &type) {
            type_ = type;
            syncToGlobalProperties();
        }

        void setSubtype(const std::string &subtype) {
            subtype_ = subtype;
            syncToGlobalProperties();
        }

        void setId(const UUID &id) {
            id_ = id;
            syncToGlobalProperties();
        }

        // ========== Owner Management ==========
        void setOwnerRobot(const UUID &robot_id) {
            owner_robot_id_ = robot_id;
            syncToGlobalProperties();
        }

        const UUID &getOwnerRobot() const { return owner_robot_id_; }
        bool hasOwner() const { return !owner_robot_id_.isNull(); }

        void releaseOwnership() {
            owner_robot_id_ = UUID::null();
            syncToGlobalProperties();
        }

        // ========== Structured Element Management ==========

        // Add structured polygon element
        void addPolygonElement(const UUID &id, const std::string &name, const std::string &type,
                               const std::string &subtype, const concord::Polygon &geometry,
                               const std::unordered_map<std::string, std::string> &props = {}) {
            polygon_elements_.emplace_back(id, name, type, subtype, geometry, props);
            // Also add to underlying Vector for storage
            geoson::Vector::addElement(geometry, type, polygon_elements_.back().toProperties());
        }

        // Add structured line element
        void addLineElement(const UUID &id, const std::string &name, const std::string &type,
                            const std::string &subtype, const concord::Line &geometry,
                            const std::unordered_map<std::string, std::string> &props = {}) {
            line_elements_.emplace_back(id, name, type, subtype, geometry, props);
            // Also add to underlying Vector for storage
            geoson::Vector::addElement(geometry, type, line_elements_.back().toProperties());
        }

        // Add structured point element
        void addPointElement(const UUID &id, const std::string &name, const std::string &type,
                             const std::string &subtype, const concord::Point &geometry,
                             const std::unordered_map<std::string, std::string> &props = {}) {
            point_elements_.emplace_back(id, name, type, subtype, geometry, props);
            // Also add to underlying Vector for storage
            geoson::Vector::addElement(geometry, type, point_elements_.back().toProperties());
        }

        // Access structured elements
        const std::vector<PolygonElement> &getPolygonElements() const { return polygon_elements_; }
        const std::vector<LineElement> &getLineElements() const { return line_elements_; }
        const std::vector<PointElement> &getPointElements() const { return point_elements_; }

        // Filter elements by type/subtype
        std::vector<PolygonElement> getPolygonsByType(const std::string &type) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : polygon_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        std::vector<PolygonElement> getPolygonsBySubtype(const std::string &subtype) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : polygon_elements_) {
                if (elem.subtype == subtype)
                    result.push_back(elem);
            }
            return result;
        }

        // ========== Higher Level Operations ==========
        double area() const { return hasFieldBoundary() ? getFieldBoundary().area() : 0.0; }
        double perimeter() const { return hasFieldBoundary() ? getFieldBoundary().perimeter() : 0.0; }
        bool contains(const concord::Point &point) const {
            return hasFieldBoundary() && getFieldBoundary().contains(point);
        }
        bool hasFieldBoundary() const { return !getFieldBoundary().getPoints().empty(); }
        bool isValid() const { return hasFieldBoundary() && !name_.empty(); }

        // ========== File I/O ==========
        static Poly fromFile(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                throw std::runtime_error("File does not exist: " + file_path.string());
            }

            // Load vector data using parent class
            geoson::Vector vector_data = geoson::Vector::fromFile(file_path);

            // Create Poly instance
            Poly poly;

            // Copy vector data
            static_cast<geoson::Vector &>(poly) = vector_data;

            // Extract metadata from global properties
            auto global_props = vector_data.getGlobalProperties();

            auto name_it = global_props.find(polygrid_globals::NAME);
            if (name_it != global_props.end()) {
                poly.name_ = name_it->second;
            }

            auto type_it = global_props.find(polygrid_globals::TYPE);
            if (type_it != global_props.end()) {
                poly.type_ = type_it->second;
            }

            auto subtype_it = global_props.find(polygrid_globals::SUBTYPE);
            if (subtype_it != global_props.end()) {
                poly.subtype_ = subtype_it->second;
            }

            auto uuid_it = global_props.find(polygrid_globals::UUID);
            if (uuid_it != global_props.end()) {
                poly.id_ = UUID(uuid_it->second);
            }

            auto owner_it = global_props.find(polygrid_globals::OWNER_ROBOT_ID);
            if (owner_it != global_props.end()) {
                poly.owner_robot_id_ = UUID(owner_it->second);
            }

            // Load structured elements from Vector elements
            poly.loadStructuredElements();

            return poly;
        }

        void toFile(const std::filesystem::path &file_path, geoson::CRS crs = geoson::CRS::ENU) const {
            // Ensure global properties are synced
            const_cast<Poly *>(this)->syncToGlobalProperties();

            // Use parent class to save
            geoson::Vector::toFile(file_path, crs);
        }

      private:
        void syncToGlobalProperties() {
            setGlobalProperty(polygrid_globals::NAME, name_);
            setGlobalProperty(polygrid_globals::TYPE, type_);
            setGlobalProperty(polygrid_globals::SUBTYPE, subtype_);
            setGlobalProperty(polygrid_globals::UUID, id_.toString());
            setGlobalProperty(polygrid_globals::OWNER_ROBOT_ID, owner_robot_id_.toString());
        }

        // Load structured elements from underlying Vector elements
        void loadStructuredElements() {
            polygon_elements_.clear();
            line_elements_.clear();
            point_elements_.clear();

            // Process all elements from underlying Vector
            for (size_t i = 0; i < elementCount(); ++i) {
                const auto &element = getElement(i);

                // Only process elements that have all required structured fields
                if (!StructuredElement::isValid(element)) {
                    continue; // Skip elements that don't have UUID, NAME, TYPE, SUBTYPE
                }

                auto structured = StructuredElement::fromElement(element);
                if (!structured.has_value())
                    continue;

                // Extract geometry and create appropriate typed element
                if (std::holds_alternative<concord::Polygon>(element.geometry)) {
                    auto polygon = std::get<concord::Polygon>(element.geometry);
                    polygon_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                   structured->subtype, polygon, structured->properties);
                } else if (std::holds_alternative<concord::Line>(element.geometry)) {
                    auto line = std::get<concord::Line>(element.geometry);
                    line_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                structured->subtype, line, structured->properties);
                } else if (std::holds_alternative<concord::Point>(element.geometry)) {
                    auto point = std::get<concord::Point>(element.geometry);
                    point_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                 structured->subtype, point, structured->properties);
                }
            }
        }
    };

    // Enhanced Grid class - High-level Raster with zone-specific functionality
    class Grid : public geotiv::Raster {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::string subtype_;
        UUID owner_robot_id_ = UUID::null();

      public:
        // ========== Constructors ==========
        Grid() : geotiv::Raster(), id_(generateUUID()), type_("other"), subtype_("default") {
        }

        Grid(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : geotiv::Raster(), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
        }

        Grid(const std::string &name, const std::string &type, const std::string &subtype, const concord::Datum &datum)
            : geotiv::Raster(datum), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
        }

        Grid(const std::string &name, const std::string &type, const std::string &subtype, const concord::Datum &datum,
             const concord::Euler &heading, geotiv::CRS crs, double resolution)
            : geotiv::Raster(datum, heading, crs, resolution), id_(generateUUID()), name_(name), type_(type),
              subtype_(subtype) {
        }

        // ========== Basic Properties ==========
        const UUID &getId() const { return id_; }
        const std::string &getName() const { return name_; }
        const std::string &getType() const { return type_; }
        const std::string &getSubtype() const { return subtype_; }

        void setName(const std::string &name) {
            name_ = name;
            syncToGlobalProperties();
        }

        void setType(const std::string &type) {
            type_ = type;
            syncToGlobalProperties();
        }

        void setSubtype(const std::string &subtype) {
            subtype_ = subtype;
            syncToGlobalProperties();
        }

        void setId(const UUID &id) {
            id_ = id;
            syncToGlobalProperties();
        }

        // ========== Owner Management ==========
        void setOwnerRobot(const UUID &robot_id) {
            owner_robot_id_ = robot_id;
            syncToGlobalProperties();
        }

        const UUID &getOwnerRobot() const { return owner_robot_id_; }
        bool hasOwner() const { return !owner_robot_id_.isNull(); }

        void releaseOwnership() {
            owner_robot_id_ = UUID::null();
            syncToGlobalProperties();
        }

        // ========== Higher Level Operations ==========
        bool isValid() const { return hasGrids() && !name_.empty(); }

        // ========== File I/O ==========
        static Grid fromFile(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                throw std::runtime_error("File does not exist: " + file_path.string());
            }

            // Load raster data using parent class
            geotiv::Raster raster_data = geotiv::Raster::fromFile(file_path);

            // Create Grid instance
            Grid grid;

            // Copy raster data
            static_cast<geotiv::Raster &>(grid) = raster_data;

            // Extract metadata from global properties
            auto global_props = raster_data.getGlobalProperties();

            auto name_it = global_props.find(polygrid_globals::NAME);
            if (name_it != global_props.end()) {
                grid.name_ = name_it->second;
            }

            auto type_it = global_props.find(polygrid_globals::TYPE);
            if (type_it != global_props.end()) {
                grid.type_ = type_it->second;
            }

            auto subtype_it = global_props.find(polygrid_globals::SUBTYPE);
            if (subtype_it != global_props.end()) {
                grid.subtype_ = subtype_it->second;
            }

            auto uuid_it = global_props.find(polygrid_globals::UUID);
            if (uuid_it != global_props.end()) {
                grid.id_ = UUID(uuid_it->second);
            }

            auto owner_it = global_props.find(polygrid_globals::OWNER_ROBOT_ID);
            if (owner_it != global_props.end()) {
                grid.owner_robot_id_ = UUID(owner_it->second);
            }

            return grid;
        }

        void toFile(const std::filesystem::path &file_path) const {
            // Ensure global properties are synced
            const_cast<Grid *>(this)->syncToGlobalProperties();

            // Use parent class to save
            geotiv::Raster::toFile(file_path);
        }

        // Override addGrid to sync properties after adding grids
        void addGrid(uint32_t width, uint32_t height, const std::string& name, 
                    const std::string& type = "", 
                    const std::unordered_map<std::string, std::string>& properties = {}) {
            geotiv::Raster::addGrid(width, height, name, type, properties);
            syncToGlobalProperties();
        }


      private:
        void syncToGlobalProperties() {
            if (hasGrids()) {
                setGlobalProperty(polygrid_globals::NAME, name_);
                setGlobalProperty(polygrid_globals::TYPE, type_);
                setGlobalProperty(polygrid_globals::SUBTYPE, subtype_);
                setGlobalProperty(polygrid_globals::UUID, id_.toString());
                setGlobalProperty(polygrid_globals::OWNER_ROBOT_ID, owner_robot_id_.toString());
            }
        }
    };

    // ========== Combined File I/O Functions ==========

    // Load both Poly and Grid from separate files
    std::pair<Poly, Grid> loadPolyGrid(const std::filesystem::path &vector_path,
                                       const std::filesystem::path &raster_path) {
        Poly poly;
        Grid grid;

        std::string vector_name, vector_uuid;
        std::string raster_name, raster_uuid;

        // Load vector data if exists
        if (std::filesystem::exists(vector_path)) {
            poly = Poly::fromFile(vector_path);
            vector_name = poly.getName();
            vector_uuid = poly.getId().toString();
        }

        // Load raster data if exists
        if (std::filesystem::exists(raster_path)) {
            grid = Grid::fromFile(raster_path);
            raster_name = grid.getName();
            raster_uuid = grid.getId().toString();
        }

        // Validate UUID and NAME consistency
        if (!vector_uuid.empty() && !raster_uuid.empty()) {
            if (vector_uuid != raster_uuid) {
                throw std::runtime_error("UUID mismatch between vector (" + vector_uuid + ") and raster (" +
                                         raster_uuid + ") data files");
            }
        }

        if (!vector_name.empty() && !raster_name.empty()) {
            if (vector_name != raster_name) {
                throw std::runtime_error("Name mismatch between vector ('" + vector_name + "') and raster ('" +
                                         raster_name + "') data files");
            }
        }

        return {poly, grid};
    }

    // Save both Poly and Grid to separate files
    void savePolyGrid(const Poly &poly, const Grid &grid, const std::filesystem::path &vector_path,
                      const std::filesystem::path &raster_path, geoson::CRS crs = geoson::CRS::ENU) {
        // TODO: Re-enable validation after debugging
        // Validate consistency before saving
        // if (poly.getId() != grid.getId()) {
        //     throw std::runtime_error("UUID mismatch between Poly and Grid: cannot save inconsistent data");
        // }

        // if (poly.getName() != grid.getName()) {
        //     throw std::runtime_error("Name mismatch between Poly and Grid: cannot save inconsistent data");
        // }

        // Save both files
        poly.toFile(vector_path, crs);
        if (grid.hasGrids()) {
            grid.toFile(raster_path);
        }
    }

} // namespace zoneout
