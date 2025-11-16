#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "geoson/vector.hpp"
#include "utils/meta.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    struct StructuredElement {
        UUID uuid;
        std::string name;
        std::string type;
        std::string subtype;
        std::unordered_map<std::string, std::string> properties;

        StructuredElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                          const std::unordered_map<std::string, std::string> &props = {});

        static bool isValid(const geoson::Element &element);

        static std::optional<StructuredElement> fromElement(const geoson::Element &element);

        std::unordered_map<std::string, std::string> toProperties() const;
    };

    struct PolygonElement : public StructuredElement {
        concord::Polygon geometry;

        PolygonElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                       const concord::Polygon &geom, const std::unordered_map<std::string, std::string> &props = {});
    };

    struct LineElement : public StructuredElement {
        concord::Line geometry;

        LineElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                    const concord::Line &geom, const std::unordered_map<std::string, std::string> &props = {});
    };

    struct PointElement : public StructuredElement {
        concord::Point geometry;

        PointElement(const UUID &id, const std::string &n, const std::string &t, const std::string &st,
                     const concord::Point &geom, const std::unordered_map<std::string, std::string> &props = {});
    };

    class Poly : public geoson::Vector {
      private:
        Meta meta_;

        std::vector<PolygonElement> polygon_elements_;
        std::vector<LineElement> line_elements_;
        std::vector<PointElement> point_elements_;

      public:
        Poly();

        Poly(const std::string &name, const std::string &type, const std::string &subtype = "default");

        Poly(const std::string &name, const std::string &type, const std::string &subtype,
             const concord::Polygon &boundary);

        Poly(const std::string &name, const std::string &type, const std::string &subtype,
             const concord::Polygon &boundary, const concord::Datum &datum, const concord::Euler &heading,
             geoson::CRS crs);

        const UUID &get_id() const;
        const std::string &get_name() const;
        const std::string &get_type() const;
        const std::string &get_subtype() const;

        void set_name(const std::string &name);

        void set_type(const std::string &type);

        void set_subtype(const std::string &subtype);

        void set_id(const UUID &id);

        void add_polygon_element(const UUID &id, const std::string &name, const std::string &type,
                                 const std::string &subtype, const concord::Polygon &geometry,
                                 const std::unordered_map<std::string, std::string> &props = {});

        void add_line_element(const UUID &id, const std::string &name, const std::string &type,
                              const std::string &subtype, const concord::Line &geometry,
                              const std::unordered_map<std::string, std::string> &props = {});

        void add_point_element(const UUID &id, const std::string &name, const std::string &type,
                               const std::string &subtype, const concord::Point &geometry,
                               const std::unordered_map<std::string, std::string> &props = {});

        const std::vector<PolygonElement> &get_polygon_elements() const;
        const std::vector<LineElement> &get_line_elements() const;
        const std::vector<PointElement> &get_point_elements() const;

        std::vector<PolygonElement> get_polygons_by_type(const std::string &type) const;

        std::vector<PolygonElement> get_polygons_by_subtype(const std::string &subtype) const;

        double area() const;
        double perimeter() const;
        bool contains(const concord::Point &point) const;
        bool has_field_boundary() const;
        bool is_valid() const;

        static Poly from_file(const std::filesystem::path &file_path);

        void to_file(const std::filesystem::path &file_path, geoson::CRS crs = geoson::CRS::WGS) const;

      private:
        void sync_to_global_properties();

        void load_structured_elements();
    };

} // namespace zoneout
