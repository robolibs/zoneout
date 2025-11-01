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
        UUID id_;
        std::string name_;
        std::string type_;
        std::string subtype_;

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

        const UUID &getId() const;
        const std::string &getName() const;
        const std::string &getType() const;
        const std::string &getSubtype() const;

        void setName(const std::string &name);

        void setType(const std::string &type);

        void setSubtype(const std::string &subtype);

        void setId(const UUID &id);

        void addPolygonElement(const UUID &id, const std::string &name, const std::string &type,
                               const std::string &subtype, const concord::Polygon &geometry,
                               const std::unordered_map<std::string, std::string> &props = {});

        void addLineElement(const UUID &id, const std::string &name, const std::string &type,
                            const std::string &subtype, const concord::Line &geometry,
                            const std::unordered_map<std::string, std::string> &props = {});

        void addPointElement(const UUID &id, const std::string &name, const std::string &type,
                             const std::string &subtype, const concord::Point &geometry,
                             const std::unordered_map<std::string, std::string> &props = {});

        const std::vector<PolygonElement> &getPolygonElements() const;
        const std::vector<LineElement> &getLineElements() const;
        const std::vector<PointElement> &getPointElements() const;

        std::vector<PolygonElement> getPolygonsByType(const std::string &type) const;

        std::vector<PolygonElement> getPolygonsBySubtype(const std::string &subtype) const;

        double area() const;
        double perimeter() const;
        bool contains(const concord::Point &point) const;
        bool hasFieldBoundary() const;
        bool isValid() const;

        static Poly fromFile(const std::filesystem::path &file_path);

        void toFile(const std::filesystem::path &file_path, geoson::CRS crs = geoson::CRS::WGS) const;

      private:
        void syncToGlobalProperties();

        void loadStructuredElements();
    };

} // namespace zoneout
