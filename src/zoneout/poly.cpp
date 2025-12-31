#include "zoneout/zoneout/poly.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "zoneout/zoneout/utils/uuid.hpp"
#include <geoson/geoson.hpp>

namespace zoneout {

    // ========== StructuredElement Methods ==========

    StructuredElement::StructuredElement(const UUID &id, const std::string &n, const std::string &t,
                                         const std::string &st,
                                         const std::unordered_map<std::string, std::string> &props)
        : uuid(id), name(n), type(t), subtype(st), properties(props) {}

    bool StructuredElement::isValid(const geoson::Feature &feature) {
        const auto &props = feature.properties;
        return props.find("uuid") != props.end() && props.find("name") != props.end() &&
               props.find("type") != props.end() && props.find("subtype") != props.end();
    }

    std::optional<StructuredElement> StructuredElement::fromFeature(const geoson::Feature &feature) {
        if (!isValid(feature))
            return std::nullopt;

        const auto &props = feature.properties;
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
                                   const dp::Polygon &geom, const std::unordered_map<std::string, std::string> &props)
        : StructuredElement(id, n, t, st, props), geometry(geom) {}

    // ========== LineElement Methods ==========

    LineElement::LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                             const dp::Segment &geom, const std::unordered_map<std::string, std::string> &props)
        : StructuredElement(id, n, t, st, props), geometry(geom) {}

    // ========== PointElement Methods ==========

    PointElement::PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                               const dp::Point &geom, const std::unordered_map<std::string, std::string> &props)
        : StructuredElement(id, n, t, st, props), geometry(geom) {}

    // ========== Poly Constructors ==========

    Poly::Poly() : collection_(), field_boundary_(), meta_("", "other", "default") { sync_to_global_properties(); }

    Poly::Poly(const std::string &name, const std::string &type, const std::string &subtype)
        : collection_(), field_boundary_(), meta_(name, type, subtype) {
        sync_to_global_properties();
        load_structured_elements();
    }

    Poly::Poly(const std::string &name, const std::string &type, const std::string &subtype,
               const dp::Polygon &boundary)
        : collection_(), field_boundary_(boundary), meta_(name, type, subtype) {
        sync_to_global_properties();
        load_structured_elements();
    }

    Poly::Poly(const std::string &name, const std::string &type, const std::string &subtype,
               const dp::Polygon &boundary, const dp::Geo &datum, const dp::Euler &heading, geoson::CRS crs)
        : collection_(), field_boundary_(boundary), meta_(name, type, subtype) {
        collection_.datum = datum;
        collection_.heading = heading;
        sync_to_global_properties();
        load_structured_elements();
    }

    // ========== FeatureCollection Access ==========

    geoson::FeatureCollection &Poly::collection() { return collection_; }
    const geoson::FeatureCollection &Poly::collection() const { return collection_; }

    // ========== Field Boundary Access ==========

    const dp::Polygon &Poly::get_field_boundary() const { return field_boundary_; }
    void Poly::set_field_boundary(const dp::Polygon &boundary) { field_boundary_ = boundary; }

    // ========== Datum and Heading Access ==========

    const dp::Geo &Poly::get_datum() const { return collection_.datum; }
    void Poly::set_datum(const dp::Geo &datum) { collection_.datum = datum; }
    const dp::Euler &Poly::get_heading() const { return collection_.heading; }
    void Poly::set_heading(const dp::Euler &heading) { collection_.heading = heading; }

    // ========== Global Properties Access ==========

    void Poly::set_global_property(const std::string &key, const std::string &value) {
        collection_.global_properties[key] = value;
    }

    std::string Poly::get_global_property(const std::string &key) const {
        auto it = collection_.global_properties.find(key);
        if (it != collection_.global_properties.end()) {
            return it->second;
        }
        return "";
    }

    const std::unordered_map<std::string, std::string> &Poly::get_global_properties() const {
        return collection_.global_properties;
    }

    // ========== Feature Management ==========

    void Poly::add_feature(const geoson::Feature &feature) { collection_.features.push_back(feature); }

    size_t Poly::feature_count() const { return collection_.features.size(); }

    const geoson::Feature &Poly::get_feature(size_t index) const { return collection_.features.at(index); }

    // ========== Field Boundary Properties ==========

    void Poly::set_field_property(const std::string &key, const std::string &value) {
        // Find the field boundary feature and set property
        for (auto &feature : collection_.features) {
            auto border_it = feature.properties.find("border");
            if (border_it != feature.properties.end() && border_it->second == "true") {
                feature.properties[key] = value;
                return;
            }
        }
    }

    // ========== Basic Properties ==========

    const UUID &Poly::get_id() const { return meta_.id; }
    const std::string &Poly::get_name() const { return meta_.name; }
    const std::string &Poly::get_type() const { return meta_.type; }
    const std::string &Poly::get_subtype() const { return meta_.subtype; }

    void Poly::set_name(const std::string &name) {
        meta_.name = name;
        sync_to_global_properties();
    }

    void Poly::set_type(const std::string &type) {
        meta_.type = type;
        sync_to_global_properties();
    }

    void Poly::set_subtype(const std::string &subtype) {
        meta_.subtype = subtype;
        sync_to_global_properties();
    }

    void Poly::set_id(const UUID &id) {
        meta_.id = id;
        sync_to_global_properties();
    }

    // ========== Structured Element Management ==========

    void Poly::add_polygon_element(const UUID &id, const std::string &name, const std::string &type,
                                   const std::string &subtype, const dp::Polygon &geometry,
                                   const std::unordered_map<std::string, std::string> &props) {
        polygon_elements_.emplace_back(id, name, type, subtype, geometry, props);
        geoson::Feature feature;
        feature.geometry = geometry;
        feature.properties = polygon_elements_.back().toProperties();
        collection_.features.push_back(feature);
    }

    void Poly::add_line_element(const UUID &id, const std::string &name, const std::string &type,
                                const std::string &subtype, const dp::Segment &geometry,
                                const std::unordered_map<std::string, std::string> &props) {
        line_elements_.emplace_back(id, name, type, subtype, geometry, props);
        geoson::Feature feature;
        feature.geometry = geometry;
        feature.properties = line_elements_.back().toProperties();
        collection_.features.push_back(feature);
    }

    void Poly::add_point_element(const UUID &id, const std::string &name, const std::string &type,
                                 const std::string &subtype, const dp::Point &geometry,
                                 const std::unordered_map<std::string, std::string> &props) {
        point_elements_.emplace_back(id, name, type, subtype, geometry, props);
        geoson::Feature feature;
        feature.geometry = geometry;
        feature.properties = point_elements_.back().toProperties();
        collection_.features.push_back(feature);
    }

    const std::vector<PolygonElement> &Poly::get_polygon_elements() const { return polygon_elements_; }
    const std::vector<LineElement> &Poly::get_line_elements() const { return line_elements_; }
    const std::vector<PointElement> &Poly::get_point_elements() const { return point_elements_; }

    std::vector<PolygonElement> Poly::get_polygons_by_type(const std::string &type) const {
        std::vector<PolygonElement> result;
        for (const auto &elem : polygon_elements_) {
            if (elem.type == type)
                result.push_back(elem);
        }
        return result;
    }

    std::vector<PolygonElement> Poly::get_polygons_by_subtype(const std::string &subtype) const {
        std::vector<PolygonElement> result;
        for (const auto &elem : polygon_elements_) {
            if (elem.subtype == subtype)
                result.push_back(elem);
        }
        return result;
    }

    // ========== Higher Level Operations ==========

    double Poly::area() const { return has_field_boundary() ? field_boundary_.area() : 0.0; }
    double Poly::perimeter() const { return has_field_boundary() ? field_boundary_.perimeter() : 0.0; }
    bool Poly::contains(const dp::Point &point) const {
        return has_field_boundary() && field_boundary_.contains(point);
    }
    bool Poly::has_field_boundary() const { return !field_boundary_.vertices.empty(); }
    bool Poly::is_valid() const { return has_field_boundary() && !meta_.name.empty(); }

    // ========== File I/O ==========

    Poly Poly::from_file(const std::filesystem::path &file_path) {
        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("File does not exist: " + file_path.string());
        }

        geoson::FeatureCollection fc = geoson::read(file_path);

        Poly poly;
        poly.collection_ = fc;

        // Extract global properties
        auto global_props = fc.global_properties;

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

        // Find and extract field boundary from features
        for (const auto &feature : fc.features) {
            auto border_it = feature.properties.find("border");
            if (border_it != feature.properties.end() && border_it->second == "true") {
                if (std::holds_alternative<dp::Polygon>(feature.geometry)) {
                    poly.field_boundary_ = std::get<dp::Polygon>(feature.geometry);
                    break;
                }
            }
        }

        poly.load_structured_elements();

        return poly;
    }

    void Poly::to_file(const std::filesystem::path &file_path, geoson::CRS crs) const {
        const_cast<Poly *>(this)->sync_to_global_properties();

        // Add field boundary as a feature if it exists
        if (has_field_boundary()) {
            // Check if boundary feature already exists
            bool boundary_exists = false;
            for (auto &feature : const_cast<Poly *>(this)->collection_.features) {
                auto border_it = feature.properties.find("border");
                if (border_it != feature.properties.end() && border_it->second == "true") {
                    boundary_exists = true;
                    feature.properties["uuid"] = meta_.id.toString();
                    feature.properties["name"] = meta_.name + "_boundary";
                    feature.properties["subtype"] = meta_.subtype;
                    break;
                }
            }

            // Add boundary feature if it doesn't exist
            if (!boundary_exists) {
                geoson::Feature boundary_feature;
                boundary_feature.geometry = field_boundary_;
                boundary_feature.properties["border"] = "true";
                boundary_feature.properties["uuid"] = meta_.id.toString();
                boundary_feature.properties["name"] = meta_.name + "_boundary";
                boundary_feature.properties["subtype"] = meta_.subtype;
                const_cast<Poly *>(this)->collection_.features.push_back(boundary_feature);
            }
        }

        geoson::write(collection_, file_path, crs);
    }

    // ========== Private Methods ==========

    void Poly::sync_to_global_properties() {
        collection_.global_properties["name"] = meta_.name;
        collection_.global_properties["type"] = meta_.type;
        collection_.global_properties["subtype"] = meta_.subtype;
        collection_.global_properties["uuid"] = meta_.id.toString();
    }

    void Poly::load_structured_elements() {
        polygon_elements_.clear();
        line_elements_.clear();
        point_elements_.clear();

        for (size_t i = 0; i < collection_.features.size(); ++i) {
            const auto &feature = collection_.features[i];

            if (!StructuredElement::isValid(feature)) {
                continue;
            }

            auto structured = StructuredElement::fromFeature(feature);
            if (!structured.has_value())
                continue;

            if (std::holds_alternative<dp::Polygon>(feature.geometry)) {
                auto polygon = std::get<dp::Polygon>(feature.geometry);
                polygon_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                               structured->subtype, polygon, structured->properties);
            } else if (std::holds_alternative<dp::Segment>(feature.geometry)) {
                auto segment = std::get<dp::Segment>(feature.geometry);
                line_elements_.emplace_back(structured->uuid, structured->name, structured->type, structured->subtype,
                                            segment, structured->properties);
            } else if (std::holds_alternative<dp::Point>(feature.geometry)) {
                auto point = std::get<dp::Point>(feature.geometry);
                point_elements_.emplace_back(structured->uuid, structured->name, structured->type, structured->subtype,
                                             point, structured->properties);
            }
        }
    }

} // namespace zoneout
