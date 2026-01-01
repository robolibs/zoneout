#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <geoson/geoson.hpp>

#include "utils/meta.hpp"
#include "utils/uuid.hpp"

namespace dp = datapod;

namespace zoneout {

    struct StructuredElement {
        UUID uuid;
        std::string name;
        std::string type;
        std::string subtype;
        std::unordered_map<std::string, std::string> properties;

        inline StructuredElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                                 const std::unordered_map<std::string, std::string> &props = {})
            : uuid(id), name(n), type(t), subtype(st), properties(props) {}

        inline static bool isValid(const geoson::Feature &feature) {
            const auto &props = feature.properties;
            return props.find("uuid") != props.end() && props.find("name") != props.end() &&
                   props.find("type") != props.end() && props.find("subtype") != props.end();
        }

        inline static std::optional<StructuredElement> fromFeature(const geoson::Feature &feature) {
            if (!isValid(feature))
                return std::nullopt;

            const auto &props = feature.properties;
            return StructuredElement(UUID(props.at("uuid")), props.at("name"), props.at("type"), props.at("subtype"),
                                     props);
        }

        inline std::unordered_map<std::string, std::string> toProperties() const {
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
    };

    struct PolygonElement : public StructuredElement {
        dp::Polygon geometry;

        inline PolygonElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                              const dp::Polygon &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    struct LineElement : public StructuredElement {
        dp::Segment geometry;

        inline LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                           const dp::Segment &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    struct PointElement : public StructuredElement {
        dp::Point geometry;

        inline PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                            const dp::Point &geom, const std::unordered_map<std::string, std::string> &props = {})
            : StructuredElement(id, n, t, st, props), geometry(geom) {}
    };

    class Poly {
      private:
        geoson::FeatureCollection collection_;
        dp::Polygon field_boundary_;
        Meta meta_;

        std::vector<PolygonElement> polygon_elements_;
        std::vector<LineElement> line_elements_;
        std::vector<PointElement> point_elements_;

        inline void sync_to_global_properties() {
            collection_.global_properties["name"] = meta_.name;
            collection_.global_properties["type"] = meta_.type;
            collection_.global_properties["subtype"] = meta_.subtype;
            collection_.global_properties["uuid"] = meta_.id.toString();
        }

        inline void load_structured_elements() {
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
                    line_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                structured->subtype, segment, structured->properties);
                } else if (std::holds_alternative<dp::Point>(feature.geometry)) {
                    auto point = std::get<dp::Point>(feature.geometry);
                    point_elements_.emplace_back(structured->uuid, structured->name, structured->type,
                                                 structured->subtype, point, structured->properties);
                }
            }
        }

      public:
        inline Poly() : collection_(), field_boundary_(), meta_("", "other", "default") { sync_to_global_properties(); }

        inline Poly(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : collection_(), field_boundary_(), meta_(name, type, subtype) {
            sync_to_global_properties();
            load_structured_elements();
        }

        inline Poly(const std::string &name, const std::string &type, const std::string &subtype,
                    const dp::Polygon &boundary)
            : collection_(), field_boundary_(boundary), meta_(name, type, subtype) {
            sync_to_global_properties();
            load_structured_elements();
        }

        inline Poly(const std::string &name, const std::string &type, const std::string &subtype,
                    const dp::Polygon &boundary, const dp::Geo &datum, const dp::Euler &heading, geoson::CRS crs)
            : collection_(), field_boundary_(boundary), meta_(name, type, subtype) {
            collection_.datum = datum;
            collection_.heading = heading;
            sync_to_global_properties();
            load_structured_elements();
        }

        // Access to the underlying FeatureCollection
        inline geoson::FeatureCollection &collection() { return collection_; }
        inline const geoson::FeatureCollection &collection() const { return collection_; }

        // Field boundary access
        inline const dp::Polygon &get_field_boundary() const { return field_boundary_; }
        inline void set_field_boundary(const dp::Polygon &boundary) { field_boundary_ = boundary; }

        // Datum and heading access
        inline const dp::Geo &get_datum() const { return collection_.datum; }
        inline void set_datum(const dp::Geo &datum) { collection_.datum = datum; }
        inline const dp::Euler &get_heading() const { return collection_.heading; }
        inline void set_heading(const dp::Euler &heading) { collection_.heading = heading; }

        // Global properties access
        inline void set_global_property(const std::string &key, const std::string &value) {
            collection_.global_properties[key] = value;
        }

        inline std::string get_global_property(const std::string &key) const {
            auto it = collection_.global_properties.find(key);
            if (it != collection_.global_properties.end()) {
                return it->second;
            }
            return "";
        }

        inline const std::unordered_map<std::string, std::string> &get_global_properties() const {
            return collection_.global_properties;
        }

        // Feature management
        inline void add_feature(const geoson::Feature &feature) { collection_.features.push_back(feature); }
        inline size_t feature_count() const { return collection_.features.size(); }
        inline const geoson::Feature &get_feature(size_t index) const { return collection_.features.at(index); }

        // Field boundary properties
        inline void set_field_property(const std::string &key, const std::string &value) {
            for (auto &feature : collection_.features) {
                auto border_it = feature.properties.find("border");
                if (border_it != feature.properties.end() && border_it->second == "true") {
                    feature.properties[key] = value;
                    return;
                }
            }
        }

        inline const UUID &get_id() const { return meta_.id; }
        inline const std::string &get_name() const { return meta_.name; }
        inline const std::string &get_type() const { return meta_.type; }
        inline const std::string &get_subtype() const { return meta_.subtype; }

        inline void set_name(const std::string &name) {
            meta_.name = name;
            sync_to_global_properties();
        }

        inline void set_type(const std::string &type) {
            meta_.type = type;
            sync_to_global_properties();
        }

        inline void set_subtype(const std::string &subtype) {
            meta_.subtype = subtype;
            sync_to_global_properties();
        }

        inline void set_id(const UUID &id) {
            meta_.id = id;
            sync_to_global_properties();
        }

        inline void add_polygon_element(const UUID &id, const std::string &name, const std::string &type,
                                        const std::string &subtype, const dp::Polygon &geometry,
                                        const std::unordered_map<std::string, std::string> &props = {}) {
            polygon_elements_.emplace_back(id, name, type, subtype, geometry, props);
            geoson::Feature feature;
            feature.geometry = geometry;
            feature.properties = polygon_elements_.back().toProperties();
            collection_.features.push_back(feature);
        }

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        inline void add_polygon_element(const dp::Polygon &geometry, const std::string &type,
                                        const std::unordered_map<std::string, std::string> &props = {}) {
            add_polygon_element(generateUUID(), type, type, "default", geometry, props);
        }

        inline void add_line_element(const UUID &id, const std::string &name, const std::string &type,
                                     const std::string &subtype, const dp::Segment &geometry,
                                     const std::unordered_map<std::string, std::string> &props = {}) {
            line_elements_.emplace_back(id, name, type, subtype, geometry, props);
            geoson::Feature feature;
            feature.geometry = geometry;
            feature.properties = line_elements_.back().toProperties();
            collection_.features.push_back(feature);
        }

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        inline void add_line_element(const dp::Segment &geometry, const std::string &type,
                                     const std::unordered_map<std::string, std::string> &props = {}) {
            add_line_element(generateUUID(), type, type, "default", geometry, props);
        }

        inline void add_point_element(const UUID &id, const std::string &name, const std::string &type,
                                      const std::string &subtype, const dp::Point &geometry,
                                      const std::unordered_map<std::string, std::string> &props = {}) {
            point_elements_.emplace_back(id, name, type, subtype, geometry, props);
            geoson::Feature feature;
            feature.geometry = geometry;
            feature.properties = point_elements_.back().toProperties();
            collection_.features.push_back(feature);
        }

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        inline void add_point_element(const dp::Point &geometry, const std::string &type,
                                      const std::unordered_map<std::string, std::string> &props = {}) {
            add_point_element(generateUUID(), type, type, "default", geometry, props);
        }

        inline const std::vector<PolygonElement> &get_polygon_elements() const { return polygon_elements_; }
        inline const std::vector<LineElement> &get_line_elements() const { return line_elements_; }
        inline const std::vector<PointElement> &get_point_elements() const { return point_elements_; }

        inline std::vector<PolygonElement> get_polygons_by_type(const std::string &type) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : polygon_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        inline std::vector<LineElement> get_lines_by_type(const std::string &type) const {
            std::vector<LineElement> result;
            for (const auto &elem : line_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        inline std::vector<PointElement> get_points_by_type(const std::string &type) const {
            std::vector<PointElement> result;
            for (const auto &elem : point_elements_) {
                if (elem.type == type)
                    result.push_back(elem);
            }
            return result;
        }

        inline std::vector<PolygonElement> get_polygons_by_subtype(const std::string &subtype) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : polygon_elements_) {
                if (elem.subtype == subtype)
                    result.push_back(elem);
            }
            return result;
        }

        // Higher Level Operations
        inline double area() const { return has_field_boundary() ? field_boundary_.area() : 0.0; }
        inline double perimeter() const { return has_field_boundary() ? field_boundary_.perimeter() : 0.0; }
        inline bool contains(const dp::Point &point) const {
            return has_field_boundary() && field_boundary_.contains(point);
        }
        inline bool has_field_boundary() const { return !field_boundary_.vertices.empty(); }
        inline bool is_valid() const { return has_field_boundary() && !meta_.name.empty(); }

        // File I/O
        inline static Poly from_file(const std::filesystem::path &file_path) {
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

        inline void to_file(const std::filesystem::path &file_path, geoson::CRS crs = geoson::CRS::WGS) const {
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
    };

} // namespace zoneout
