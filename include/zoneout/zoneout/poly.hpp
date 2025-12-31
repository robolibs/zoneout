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

        StructuredElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                          const std::unordered_map<std::string, std::string> &props = {});

        static bool isValid(const geoson::Feature &feature);

        static std::optional<StructuredElement> fromFeature(const geoson::Feature &feature);

        std::unordered_map<std::string, std::string> toProperties() const;
    };

    struct PolygonElement : public StructuredElement {
        dp::Polygon geometry;

        PolygonElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                       const dp::Polygon &geom, const std::unordered_map<std::string, std::string> &props = {});
    };

    struct LineElement : public StructuredElement {
        dp::Segment geometry;

        LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                    const dp::Segment &geom, const std::unordered_map<std::string, std::string> &props = {});
    };

    struct PointElement : public StructuredElement {
        dp::Point geometry;

        PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                     const dp::Point &geom, const std::unordered_map<std::string, std::string> &props = {});
    };

    class Poly {
      private:
        geoson::FeatureCollection collection_;
        dp::Polygon field_boundary_;
        Meta meta_;

        std::vector<PolygonElement> polygon_elements_;
        std::vector<LineElement> line_elements_;
        std::vector<PointElement> point_elements_;

      public:
        Poly();

        Poly(const std::string &name, const std::string &type, const std::string &subtype = "default");

        Poly(const std::string &name, const std::string &type, const std::string &subtype, const dp::Polygon &boundary);

        Poly(const std::string &name, const std::string &type, const std::string &subtype, const dp::Polygon &boundary,
             const dp::Geo &datum, const dp::Euler &heading, geoson::CRS crs);

        // Access to the underlying FeatureCollection
        geoson::FeatureCollection &collection();
        const geoson::FeatureCollection &collection() const;

        // Field boundary access
        const dp::Polygon &get_field_boundary() const;
        void set_field_boundary(const dp::Polygon &boundary);

        // Datum and heading access
        const dp::Geo &get_datum() const;
        void set_datum(const dp::Geo &datum);
        const dp::Euler &get_heading() const;
        void set_heading(const dp::Euler &heading);

        // Global properties access
        void set_global_property(const std::string &key, const std::string &value);
        std::string get_global_property(const std::string &key) const;
        const std::unordered_map<std::string, std::string> &get_global_properties() const;

        // Feature management
        void add_feature(const geoson::Feature &feature);
        size_t feature_count() const;
        const geoson::Feature &get_feature(size_t index) const;

        // Field boundary properties
        void set_field_property(const std::string &key, const std::string &value);

        const UUID &get_id() const;
        const std::string &get_name() const;
        const std::string &get_type() const;
        const std::string &get_subtype() const;

        void set_name(const std::string &name);

        void set_type(const std::string &type);

        void set_subtype(const std::string &subtype);

        void set_id(const UUID &id);

        void add_polygon_element(const UUID &id, const std::string &name, const std::string &type,
                                 const std::string &subtype, const dp::Polygon &geometry,
                                 const std::unordered_map<std::string, std::string> &props = {});

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        void add_polygon_element(const dp::Polygon &geometry, const std::string &type,
                                 const std::unordered_map<std::string, std::string> &props = {});

        void add_line_element(const UUID &id, const std::string &name, const std::string &type,
                              const std::string &subtype, const dp::Segment &geometry,
                              const std::unordered_map<std::string, std::string> &props = {});

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        void add_line_element(const dp::Segment &geometry, const std::string &type,
                              const std::unordered_map<std::string, std::string> &props = {});

        void add_point_element(const UUID &id, const std::string &name, const std::string &type,
                               const std::string &subtype, const dp::Point &geometry,
                               const std::unordered_map<std::string, std::string> &props = {});

        // Convenience overload: auto-generates UUID, uses type as name and "default" as subtype
        void add_point_element(const dp::Point &geometry, const std::string &type,
                               const std::unordered_map<std::string, std::string> &props = {});

        const std::vector<PolygonElement> &get_polygon_elements() const;
        const std::vector<LineElement> &get_line_elements() const;
        const std::vector<PointElement> &get_point_elements() const;

        std::vector<PolygonElement> get_polygons_by_type(const std::string &type) const;
        std::vector<LineElement> get_lines_by_type(const std::string &type) const;
        std::vector<PointElement> get_points_by_type(const std::string &type) const;

        std::vector<PolygonElement> get_polygons_by_subtype(const std::string &subtype) const;

        double area() const;
        double perimeter() const;
        bool contains(const dp::Point &point) const;
        bool has_field_boundary() const;
        bool is_valid() const;

        static Poly from_file(const std::filesystem::path &file_path);

        void to_file(const std::filesystem::path &file_path, geoson::CRS crs = geoson::CRS::WGS) const;

      private:
        void sync_to_global_properties();

        void load_structured_elements();
    };

} // namespace zoneout
