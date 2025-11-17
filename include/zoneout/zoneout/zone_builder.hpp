#pragma once

#include "zone.hpp"
#include <optional>
#include <vector>

namespace zoneout {

    /**
     * @brief Builder pattern for constructing Zone objects with fluent interface
     *
     * ZoneBuilder provides a fluent, expressive way to construct complex Zone objects
     * with optional configurations. It validates the configuration before building.
     *
     * Example:
     * @code
     * auto zone = ZoneBuilder()
     *     .with_name("field_1")
     *     .with_type("agricultural")
     *     .with_boundary(boundary_polygon)
     *     .with_datum({52.0, 5.0, 0.0})
     *     .with_resolution(1.0)
     *     .with_property("crop", "wheat")
     *     .with_polygon_feature(obstacle, "tree", "obstacle")
     *     .build();
     * @endcode
     */
    class ZoneBuilder {
      private:
        // Required fields
        std::optional<std::string> name_;
        std::optional<std::string> type_;
        std::optional<concord::Polygon> boundary_;
        std::optional<concord::Datum> datum_;

        // Optional fields with defaults
        double resolution_ = 1.0;
        std::optional<concord::Grid<uint8_t>> initial_grid_;

        // Collections
        std::unordered_map<std::string, std::string> properties_;

        struct RasterLayerConfig {
            concord::Grid<uint8_t> grid;
            std::string name;
            std::string type;
            std::unordered_map<std::string, std::string> properties;
            bool poly_cut;
            int layer_index;
        };
        std::vector<RasterLayerConfig> raster_layers_;

        struct PolygonFeatureConfig {
            concord::Polygon geometry;
            std::string name;
            std::string type;
            std::string subtype;
            std::unordered_map<std::string, std::string> properties;
        };
        std::vector<PolygonFeatureConfig> polygon_features_;

      public:
        ZoneBuilder() = default;

        // Required configuration methods
        ZoneBuilder &with_name(const std::string &name);
        ZoneBuilder &with_type(const std::string &type);
        ZoneBuilder &with_boundary(const concord::Polygon &boundary);
        ZoneBuilder &with_datum(const concord::Datum &datum);

        // Optional configuration methods
        ZoneBuilder &with_resolution(double resolution);
        ZoneBuilder &with_initial_grid(const concord::Grid<uint8_t> &grid);
        ZoneBuilder &with_property(const std::string &key, const std::string &value);
        ZoneBuilder &with_properties(const std::unordered_map<std::string, std::string> &properties);

        // Feature configuration methods
        ZoneBuilder &with_raster_layer(const concord::Grid<uint8_t> &grid, const std::string &name,
                                       const std::string &type = "",
                                       const std::unordered_map<std::string, std::string> &properties = {},
                                       bool poly_cut = false, int layer_index = -1);

        ZoneBuilder &with_polygon_feature(const concord::Polygon &geometry, const std::string &name,
                                          const std::string &type = "", const std::string &subtype = "default",
                                          const std::unordered_map<std::string, std::string> &properties = {});

        // Validation and building
        bool is_valid() const;
        std::string validation_error() const;
        Zone build() const;

        // Reset builder to initial state
        void reset();
    };

} // namespace zoneout
