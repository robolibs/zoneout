#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "geoson/vector.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

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
            return props.find("uuid") != props.end() &&
                   props.find("name") != props.end() &&
                   props.find("type") != props.end() &&
                   props.find("subtype") != props.end();
        }

        // Create from geoson::Element if valid
        static std::optional<StructuredElement> fromElement(const geoson::Element &element) {
            if (!isValid(element))
                return std::nullopt;

            const auto &props = element.properties;
            return StructuredElement(UUID(props.at("uuid")), props.at("name"),
                                     props.at("type"), props.at("subtype"), props);
        }

        // Convert to properties map for storage
        std::unordered_map<std::string, std::string> toProperties() const {
            auto props = properties;
            props["uuid"] = uuid.toString();
            props["name"] = name;
            props["type"] = type;
            props["subtype"] = subtype;
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

            auto name_it = global_props.find("name");
            if (name_it != global_props.end()) {
                poly.name_ = name_it->second;
            }

            auto type_it = global_props.find("type");
            if (type_it != global_props.end()) {
                poly.type_ = type_it->second;
            }

            auto subtype_it = global_props.find("subtype");
            if (subtype_it != global_props.end()) {
                poly.subtype_ = subtype_it->second;
            }

            auto uuid_it = global_props.find("uuid");
            if (uuid_it != global_props.end()) {
                poly.id_ = UUID(uuid_it->second);
            }

            // Load structured elements from Vector elements
            poly.loadStructuredElements();
            
            // If we have a :border: element, use it as the field boundary
            poly.applyBoundaryFromElements();

            return poly;
        }

        void toFile(const std::filesystem::path &file_path, geoson::CRS crs = geoson::CRS::ENU) const {
            // Ensure global properties are synced
            const_cast<Poly *>(this)->syncToGlobalProperties();
            
            // Ensure field boundary is saved as a structured element with :border: type
            const_cast<Poly *>(this)->ensureBoundaryElement();

            // Use parent class to save
            geoson::Vector::toFile(file_path, crs);
        }

      private:
        void syncToGlobalProperties() {
            setGlobalProperty("name", name_);
            setGlobalProperty("type", type_);
            setGlobalProperty("subtype", subtype_);
            setGlobalProperty("uuid", id_.toString());
        }

        // Ensure field boundary is saved as a structured element
        void ensureBoundaryElement() {
            if (!hasFieldBoundary()) return;
            
            // Check if boundary element already exists
            bool hasBoundaryElement = false;
            for (const auto &elem : polygon_elements_) {
                if (elem.type == "border") {
                    hasBoundaryElement = true;
                    break;
                }
            }
            
            // If no boundary element exists, create one
            if (!hasBoundaryElement) {
                auto boundary = getFieldBoundary();
                addPolygonElement(generateUUID(), name_ + "_boundary", "border", "field", boundary, {});
            }
        }
        
        // Apply boundary from loaded elements if border type exists
        void applyBoundaryFromElements() {
            for (const auto &elem : polygon_elements_) {
                if (elem.type == "border") {
                    // Set this polygon as the field boundary
                    setFieldBoundary(elem.geometry);
                    break;
                }
            }
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

} // namespace zoneout