#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "concord/concord.hpp"
#include "constants.hpp"
#include "entropy/generator.hpp"
#include "polygrid.hpp"
#include "utils/meta.hpp"
#include "utils/time.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    // Forward declaration
    class Zone;

    // Factory helper for creating zones with validation
    Zone make_zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
                   const concord::Datum &datum, double resolution = DEFAULT_RESOLUTION);

    class Zone {
      private:
        Poly poly_data_;
        Grid grid_data_;

        UUID id_;
        std::string name_;
        std::string type_;

        std::unordered_map<std::string, std::string> properties_;

      public:
        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Grid<uint8_t> &initial_grid, const concord::Datum &datum);

        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Datum &datum, double resolution = 1.0);

        const UUID &id() const;
        const std::string &name() const;
        const std::string &type() const;
        void set_name(const std::string &name);
        void set_type(const std::string &type);
        void set_property(const std::string &key, const std::string &value);
        std::string get_property(const std::string &key, const std::string &default_value = "") const;
        const std::unordered_map<std::string, std::string> &properties() const;
        const concord::Datum &datum() const;
        void set_datum(const concord::Datum &datum);

        void add_raster_layer(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                              const std::unordered_map<std::string, std::string> &properties = {},
                              bool poly_cut = false, int layer_index = -1);
        std::string raster_info() const;
        void add_polygon_feature(const concord::Polygon &geometry, const std::string &name,
                                 const std::string &type = "", const std::string &subtype = "default",
                                 const std::unordered_map<std::string, std::string> &properties = {});
        std::string feature_info() const;

        bool is_valid() const;

        static Zone from_files(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path);
        void to_files(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) const;
        void save(const std::filesystem::path &directory) const;
        static Zone load(const std::filesystem::path &directory);

        const geoson::Vector &vector_data() const;
        const geotiv::Raster &raster_data() const;
        geoson::Vector &vector_data();
        geotiv::Raster &raster_data();

        std::string global_property(const char *global_name) const;
        void set_global_property(const char *global_name, const std::string &value);
        void sync_to_poly_grid();

        // Accessors for internal data structures
        Poly &poly();
        const Poly &poly() const;

        Grid &grid();
        const Grid &grid() const;

      private:
    };

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
