#include "zoneout/zoneout/poly.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "geoson/vector.hpp"
#include "zoneout/zoneout/utils/uuid.hpp"

namespace zoneout {

    // ========== StructuredElement Methods ==========

    StructuredElement::StructuredElement(const UUID &id, const std::string &n, const std::string &t,
                                         const std::string &st,
                                         const std::unordered_map<std::string, std::string> &props)
        : uuid(id), name(n), type(t), subtype(st), properties(props) {}

    bool StructuredElement::isValid(const geoson::Element &element) {
        const auto &props = element.properties;
        return props.find("uuid") != props.end() && props.find("name") != props.end() &&
               props.find("type") != props.end() && props.find("subtype") != props.end();
    }

    std::optional<StructuredElement> StructuredElement::fromElement(const geoson::Element &element) {
        if (!isValid(element))
            return std::nullopt;

        const auto &props = element.properties;
        return StructuredElement(UUID(props.at("uuid")), props.at("name"), props.at("type"), props.at("subtype"),
                                 props);
    }

    std::unordered_map<std::string, std::string> StructuredElement::toProperties() const {
        auto props = properties;
        props["uuid"] = uuid.toString();
        props["name"] = name;
        props["type"] = type;
        props["subtype"] = subtype;
        if (props.find("border") == props.end()) {
            props["border"] = "false";
        }
        return props;
    }

    // ========== PolygonElement Methods ==========

    PolygonElement::PolygonElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                                   const concord::Polygon &geom,
                                   const std::unordered_map<std::string, std::string> &props)
        : StructuredElement(id, n, t, st, props), geometry(geom) {}

    // ========== LineElement Methods ==========

    LineElement::LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                             const concord::Line &geom, const std::unordered_map<std::string, std::string> &props)
        : StructuredElement(id, n, t, st, props), geometry(geom) {}

    // ========== PointElement Methods ==========

    PointElement::PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                               const concord::Point &geom, const std::unordered_map<std::string, std::string> &props)
        : StructuredElement(id, n, t, st, props), geometry(geom) {}

    // ========== Poly Constructors ==========

    Poly::Poly() : geoson::Vector(concord::Polygon{}), meta_("", "other", "default") {
        syncToGlobalProperties();
    }

    Poly::Poly(const std::string &name, const std::string &type, const std::string &subtype)
        : geoson::Vector(concord::Polygon{}), meta_(name, type, subtype) {
        syncToGlobalProperties();
        loadStructuredElements();
    }

    Poly::Poly(const std::string &name, const std::string &type, const std::string &subtype,
               const concord::Polygon &boundary)
        : geoson::Vector(boundary), meta_(name, type, subtype) {
        syncToGlobalProperties();
        loadStructuredElements();
    }

    Poly::Poly(const std::string &name, const std::string &type, const std::string &subtype,
               const concord::Polygon &boundary, const concord::Datum &datum, const concord::Euler &heading,
               geoson::CRS crs)
        : geoson::Vector(boundary, datum, heading, crs), meta_(name, type,
          subtype) {
        syncToGlobalProperties();
        loadStructuredElements();
    }

    // ========== Basic Properties ==========

    const UUID &Poly::getId() const { return meta_.id; }
    const std::string &Poly::getName() const { return meta_.name; }
    const std::string &Poly::getType() const { return meta_.type; }
    const std::string &Poly::getSubtype() const { return meta_.subtype; }

    void Poly::setName(const std::string &name) {
        meta_.name = name;
        syncToGlobalProperties();
    }

    void Poly::setType(const std::string &type) {
        meta_.type = type;
        syncToGlobalProperties();
    }

    void Poly::setSubtype(const std::string &subtype) {
        meta_.subtype = subtype;
        syncToGlobalProperties();
    }

    void Poly::setId(const UUID &id) {
        meta_.id = id;
        syncToGlobalProperties();
    }

    // ========== Structured Element Management ==========

    void Poly::addPolygonElement(const UUID &id, const std::string &name, const std::string &type,
                                 const std::string &subtype, const concord::Polygon &geometry,
                                 const std::unordered_map<std::string, std::string> &props) {
        polygon_elements_.emplace_back(id, name, type, subtype, geometry, props);
        geoson::Vector::addElement(geometry, type, polygon_elements_.back().toProperties());
    }

    void Poly::addLineElement(const UUID &id, const std::string &name, const std::string &type,
                              const std::string &subtype, const concord::Line &geometry,
                              const std::unordered_map<std::string, std::string> &props) {
        line_elements_.emplace_back(id, name, type, subtype, geometry, props);
        geoson::Vector::addElement(geometry, type, line_elements_.back().toProperties());
    }

    void Poly::addPointElement(const UUID &id, const std::string &name, const std::string &type,
                               const std::string &subtype, const concord::Point &geometry,
                               const std::unordered_map<std::string, std::string> &props) {
        point_elements_.emplace_back(id, name, type, subtype, geometry, props);
        geoson::Vector::addElement(geometry, type, point_elements_.back().toProperties());
    }

    const std::vector<PolygonElement> &Poly::getPolygonElements() const { return polygon_elements_; }
    const std::vector<LineElement> &Poly::getLineElements() const { return line_elements_; }
    const std::vector<PointElement> &Poly::getPointElements() const { return point_elements_; }

    std::vector<PolygonElement> Poly::getPolygonsByType(const std::string &type) const {
        std::vector<PolygonElement> result;
        for (const auto &elem : polygon_elements_) {
            if (elem.type == type)
                result.push_back(elem);
        }
        return result;
    }

    std::vector<PolygonElement> Poly::getPolygonsBySubtype(const std::string &subtype) const {
        std::vector<PolygonElement> result;
        for (const auto &elem : polygon_elements_) {
            if (elem.subtype == subtype)
                result.push_back(elem);
        }
        return result;
    }

    // ========== Higher Level Operations ==========

    double Poly::area() const { return hasFieldBoundary() ? getFieldBoundary().area() : 0.0; }
    double Poly::perimeter() const { return hasFieldBoundary() ? getFieldBoundary().perimeter() : 0.0; }
    bool Poly::contains(const concord::Point &point) const {
        return hasFieldBoundary() && getFieldBoundary().contains(point);
    }
    bool Poly::hasFieldBoundary() const { return !getFieldBoundary().getPoints().empty(); }
    bool Poly::isValid() const { return hasFieldBoundary() && !meta_.name.empty(); }

    // ========== File I/O ==========

    Poly Poly::fromFile(const std::filesystem::path &file_path) {
        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("File does not exist: " + file_path.string());
        }

        geoson::Vector vector_data = geoson::Vector::fromFile(file_path);

        Poly poly;

        static_cast<geoson::Vector &>(poly) = vector_data;

        auto global_props = vector_data.getGlobalProperties();

        auto name_it = global_props.find("name");
        if (name_it != global_props.end()) {
            poly.meta_.name = name_it->second;
        }

        auto type_it = global_props.find("type");
        if (type_it != global_props.end()) {
            poly.meta_.type = type_it->second;
        }

        auto subtype_it = global_props.find("subtype");
        if (subtype_it != global_props.end()) {
            poly.meta_.subtype = subtype_it->second;
        }

        auto uuid_it = global_props.find("uuid");
        if (uuid_it != global_props.end()) {
            poly.meta_.id = UUID(uuid_it->second);
        }

        poly.loadStructuredElements();

        return poly;
    }

    void Poly::toFile(const std::filesystem::path &file_path, geoson::CRS crs) const {
        const_cast<Poly *>(this)->syncToGlobalProperties();

        if (hasFieldBoundary()) {
            const_cast<Poly *>(this)->setFieldProperty("border", "true");

            const_cast<Poly *>(this)->setFieldProperty("uuid", meta_.id.toString());
            const_cast<Poly *>(this)->setFieldProperty("name", meta_.name + "_boundary");
            const_cast<Poly *>(this)->setFieldProperty("subtype", meta_.subtype);
        }

        geoson::Vector::toFile(file_path, crs);
    }

    // ========== Private Methods ==========

    void Poly::syncToGlobalProperties() {
        setGlobalProperty("name", meta_.name);
        setGlobalProperty("type", meta_.type);
        setGlobalProperty("subtype", meta_.subtype);
        setGlobalProperty("uuid", meta_.id.toString());
    }

    void Poly::loadStructuredElements() {
        polygon_elements_.clear();
        line_elements_.clear();
        point_elements_.clear();

        for (size_t i = 0; i < elementCount(); ++i) {
            const auto &element = getElement(i);

            if (!StructuredElement::isValid(element)) {
                continue;
            }

            auto structured = StructuredElement::fromElement(element);
            if (!structured.has_value())
                continue;

            if (std::holds_alternative<concord::Polygon>(element.geometry)) {
                auto polygon = std::get<concord::Polygon>(element.geometry);
                polygon_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                               structured->subtype, polygon, structured->properties);
            } else if (std::holds_alternative<concord::Line>(element.geometry)) {
                auto line = std::get<concord::Line>(element.geometry);
                line_elements_.emplace_back(structured->uuid, structured->name, structured->type, structured->subtype,
                                            line, structured->properties);
            } else if (std::holds_alternative<concord::Point>(element.geometry)) {
                auto point = std::get<concord::Point>(element.geometry);
                point_elements_.emplace_back(structured->uuid, structured->name, structured->type, structured->subtype,
                                             point, structured->properties);
            }
        }
    }

} // namespace zoneout
