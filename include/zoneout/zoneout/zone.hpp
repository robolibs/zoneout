#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <entropy/generator.hpp>
#include <rastkit/rastkit.hpp>
#include <vectkit/vectkit.hpp>

#include "constants.hpp"
#include "polygrid.hpp"
#include "utils/meta.hpp"
#include "utils/time.hpp"
#include "utils/uuid.hpp"

namespace dp = datapod;

namespace zoneout {

    // Forward declaration
    class Zone;
    class ZoneBuilder;

    class Zone {
      private:
        Poly poly_data_;
        Grid grid_data_;

        UUID id_;
        std::string name_;
        std::string type_;

        std::unordered_map<std::string, std::string> properties_;

      public:
        inline Zone(const std::string &name, const std::string &type, const dp::Polygon &boundary,
                    const dp::Grid<uint8_t> &initial_grid, const dp::Geo &datum)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
              grid_data_(name, type, "default") {
            set_datum(datum);
            auto aabb = boundary.get_aabb();
            dp::Pose grid_pose{aabb.center(), dp::Euler{0, 0, 0}.to_quaternion()};
            grid_data_.shift() = grid_pose;
            grid_data_.resolution() = initial_grid.resolution;
            grid_data_.add_grid(initial_grid, "base_layer", "terrain");
            sync_to_poly_grid();
        }

        inline Zone(const std::string &name, const std::string &type, const dp::Polygon &boundary, const dp::Geo &datum,
                    double resolution = 1.0)
            : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
              grid_data_(name, type, "default") {
            set_datum(datum);

            auto aabb = boundary.get_aabb();

            double padding = resolution * 2.0;
            dp::Point aabb_size = aabb.max_point - aabb.min_point;
            double grid_width = aabb_size.x + padding;
            double grid_height = aabb_size.y + padding;

            size_t grid_rows = static_cast<size_t>(std::ceil(grid_height / resolution));
            size_t grid_cols = static_cast<size_t>(std::ceil(grid_width / resolution));

            grid_rows = std::max(grid_rows, static_cast<size_t>(1));
            grid_cols = std::max(grid_cols, static_cast<size_t>(1));

            dp::Pose grid_pose{aabb.center(), dp::Euler{0, 0, 0}.to_quaternion()};
            dp::Grid<uint8_t> generated_grid;
            generated_grid.rows = grid_rows;
            generated_grid.cols = grid_cols;
            generated_grid.resolution = resolution;
            generated_grid.centered = true;
            generated_grid.pose = grid_pose;
            generated_grid.data.resize(grid_rows * grid_cols, 0);

            grid_data_.shift() = grid_pose;
            grid_data_.resolution() = resolution;

            entropy::noise::NoiseGen noise;
            noise.SetNoiseType(entropy::noise::NoiseGen::NoiseType_OpenSimplex2);
            auto sz = std::max(aabb_size.x, aabb_size.y);
            noise.SetFrequency(sz / 300000.0f);
            noise.SetSeed(std::random_device{}());

            for (size_t r = 0; r < generated_grid.rows; ++r) {
                for (size_t c = 0; c < generated_grid.cols; ++c) {
                    auto cell_center = generated_grid.get_point(r, c);

                    if (boundary.contains(cell_center)) {
                        generated_grid(r, c) = 255;
                    } else {
                        generated_grid(r, c) = 0;
                    }
                }
            }

            grid_data_.add_grid(generated_grid, "base_layer", "terrain");
            sync_to_poly_grid();
        }

        inline const UUID &id() const { return id_; }
        inline const std::string &name() const { return name_; }
        inline const std::string &type() const { return type_; }

        inline void set_name(const std::string &name) {
            name_ = name;
            poly_data_.set_name(name);
            grid_data_.set_name(name);
        }

        inline void set_type(const std::string &type) {
            type_ = type;
            poly_data_.set_type(type);
            grid_data_.set_type(type);
        }

        inline void set_property(const std::string &key, const std::string &value) { properties_[key] = value; }

        inline dp::Optional<std::string> property(const std::string &key) const {
            auto it = properties_.find(key);
            if (it != properties_.end())
                return it->second;
            return dp::nullopt;
        }

        inline const std::unordered_map<std::string, std::string> &properties() const { return properties_; }

        /// Remove a property by key. Returns true if the property was found and removed.
        inline bool remove_property(const std::string &key) { return properties_.erase(key) > 0; }

        /// Clear all properties
        inline void clear_properties() { properties_.clear(); }

        /// Check if a property exists
        inline bool has_property(const std::string &key) const { return properties_.find(key) != properties_.end(); }

        inline const dp::Geo &datum() const { return poly_data_.datum(); }

        inline void set_datum(const dp::Geo &datum) {
            poly_data_.set_datum(datum);
            grid_data_.datum() = datum;
        }

        inline void add_raster_layer(const dp::Grid<uint8_t> &grid, const std::string &name,
                                     const std::string &type = "",
                                     const std::unordered_map<std::string, std::string> &properties = {},
                                     bool poly_cut = false, int layer_index = -1) {
            if (poly_cut && poly_data_.has_field_boundary()) {
                auto modified_grid = grid;

                auto boundary = poly_data_.field_boundary();

                size_t cells_inside = 0;
                size_t total_cells = modified_grid.rows * modified_grid.cols;

                for (size_t r = 0; r < modified_grid.rows; ++r) {
                    for (size_t c = 0; c < modified_grid.cols; ++c) {
                        auto cell_center = modified_grid.get_point(r, c);

                        if (boundary.contains(cell_center)) {
                            cells_inside++;
                        } else {
                            modified_grid(r, c) = 0;
                        }
                    }
                }

                grid_data_.add_grid(modified_grid, name, type, properties);
            } else {
                grid_data_.add_grid(grid, name, type, properties);
            }
        }

        inline std::string raster_info() const {
            if (grid_data_.layer_count() > 0) {
                const auto &first_layer = grid_data_.get_layer(0);
                return "Raster size: " + std::to_string(first_layer.width) + "x" + std::to_string(first_layer.height) +
                       " (" + std::to_string(grid_data_.layer_count()) + " layers)";
            }
            return "No raster layers";
        }

        inline void add_polygon_element(const dp::Polygon &geometry, const std::string &name,
                                        const std::string &type = "", const std::string &subtype = "default",
                                        const std::unordered_map<std::string, std::string> &properties = {}) {
            if (poly_data_.has_field_boundary()) {
                auto boundary = poly_data_.field_boundary();

                for (const auto &point : geometry.vertices) {
                    if (!boundary.contains(point)) {
                        throw std::runtime_error("Polygon element '" + name +
                                                 "' is not valid: points must be inside field boundary");
                    }
                }
            }

            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> color_dist(50, 200);
            uint8_t polygon_color = static_cast<uint8_t>(color_dist(gen));

            if (grid_data_.layer_count() > 0) {
                auto &grid_variant = grid_data_.get_layer(0).grid;
                std::visit(
                    [&](auto &base_grid) {
                        using GridType = std::decay_t<decltype(base_grid)>;
                        if constexpr (!std::is_same_v<GridType, dp::Grid<rastkit::RGBA>>) {
                            for (size_t r = 0; r < base_grid.rows; ++r) {
                                for (size_t c = 0; c < base_grid.cols; ++c) {
                                    auto cell_center = base_grid.get_point(r, c);
                                    if (geometry.contains(cell_center)) {
                                        using CellType = typename decltype(base_grid.data)::value_type;
                                        base_grid(r, c) = static_cast<CellType>(polygon_color);
                                    }
                                }
                            }
                        }
                    },
                    grid_variant);
            }

            UUID element_id = generateUUID();

            poly_data_.add_polygon_element(element_id, name, type, subtype, geometry, properties);
        }

        inline std::string element_info() const {
            const auto &polygon_elements = poly_data_.polygon_elements();
            const auto &line_elements = poly_data_.line_elements();
            const auto &point_elements = poly_data_.point_elements();

            size_t total_elements = polygon_elements.size() + line_elements.size() + point_elements.size();

            if (total_elements > 0) {
                return "Elements: " + std::to_string(polygon_elements.size()) + " polygons, " +
                       std::to_string(line_elements.size()) + " lines, " + std::to_string(point_elements.size()) +
                       " points (" + std::to_string(total_elements) + " total)";
            }
            return "No elements";
        }

        // ============ Spatial Queries ============

        /// Check if a point is inside this zone's boundary
        inline bool contains(const dp::Point &point) const { return poly_data_.contains(point); }

        /// Get all polygon elements that intersect with the given bounding box
        inline std::vector<PolygonElement> polygon_elements_in_area(const dp::AABB &bbox) const {
            std::vector<PolygonElement> result;
            for (const auto &elem : poly_data_.polygon_elements()) {
                auto elem_aabb = elem.geometry.get_aabb();
                // Use AABB's built-in intersects method
                if (elem_aabb.intersects(bbox)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get all point elements within the given bounding box
        inline std::vector<PointElement> point_elements_in_area(const dp::AABB &bbox) const {
            std::vector<PointElement> result;
            for (const auto &elem : poly_data_.point_elements()) {
                if (bbox.contains(elem.geometry)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get all line elements that intersect with the given bounding box
        inline std::vector<LineElement> line_elements_in_area(const dp::AABB &bbox) const {
            std::vector<LineElement> result;
            for (const auto &elem : poly_data_.line_elements()) {
                // Check if either endpoint is in the bbox
                if (bbox.contains(elem.geometry.start) || bbox.contains(elem.geometry.end)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get all point elements that are inside the given polygon
        inline std::vector<PointElement> points_in_polygon(const dp::Polygon &area) const {
            std::vector<PointElement> result;
            for (const auto &elem : poly_data_.point_elements()) {
                if (area.contains(elem.geometry)) {
                    result.push_back(elem);
                }
            }
            return result;
        }

        /// Get the zone's bounding box
        inline dp::AABB bounding_box() const {
            if (poly_data_.has_field_boundary()) {
                return poly_data_.field_boundary().get_aabb();
            }
            return dp::AABB{};
        }

        inline bool is_valid() const { return poly_data_.is_valid() && grid_data_.is_valid(); }

        inline static Zone from_files(const std::filesystem::path &vector_path,
                                      const std::filesystem::path &raster_path) {
            auto [poly, grid] = loadPolyGrid(vector_path, raster_path);

            auto datum = poly.datum();

            dp::Grid<uint8_t> base_grid;
            if (grid.has_layers()) {
                base_grid = std::get<dp::Grid<uint8_t>>(grid.get_layer(0).grid);
            } else {
                dp::Pose shift{dp::Point{0.0, 0.0, 0.0}, dp::Euler{0, 0, 0}.to_quaternion()};
                base_grid.rows = 10;
                base_grid.cols = 10;
                base_grid.resolution = 1.0;
                base_grid.centered = true;
                base_grid.pose = shift;
                base_grid.data.resize(100, 0);
            }

            dp::Polygon default_boundary;
            Zone zone("", "", default_boundary, base_grid, datum);
            zone.poly_data_ = poly;
            zone.grid_data_ = grid;

            if (poly.name().empty() && !grid.name().empty()) {
                zone.name_ = grid.name();
            } else {
                zone.name_ = poly.name();
            }

            if (poly.type().empty() && !grid.type().empty()) {
                zone.type_ = grid.type();
            } else {
                zone.type_ = poly.type();
            }

            if (!poly.id().isNull()) {
                zone.id_ = poly.id();
            } else if (!grid.id().isNull()) {
                zone.id_ = grid.id();
            }

            if (std::filesystem::exists(vector_path)) {
                auto field_props = poly.global_properties();
                for (const auto &[key, value] : field_props) {
                    if (key.substr(0, 5) == "prop_") {
                        zone.set_property(key.substr(5), value);
                    }
                }
            }

            zone.sync_to_poly_grid();
            return zone;
        }

        inline void to_files(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) const {
            const_cast<Zone *>(this)->sync_to_poly_grid();

            auto poly_copy = poly_data_;
            auto grid_copy = grid_data_;

            poly_copy.set_id(id_);
            grid_copy.set_id(id_);

            for (const auto &[key, value] : properties_) {
                poly_copy.set_global_property("prop_" + key, value);
            }

            savePolyGrid(poly_copy, grid_copy, vector_path, raster_path);
        }

        inline void save(const std::filesystem::path &directory) const {
            std::filesystem::create_directories(directory);
            auto vector_path = directory / "vector.geojson";
            auto raster_path = directory / "raster.tiff";
            to_files(vector_path, raster_path);
        }

        inline static Zone load(const std::filesystem::path &directory) {
            auto vector_path = directory / "vector.geojson";
            auto raster_path = directory / "raster.tiff";
            return from_files(vector_path, raster_path);
        }

        inline const vectkit::FeatureCollection &vector_data() const { return poly_data_.collection(); }
        inline const rastkit::RasterCollection &raster_data() const { return grid_data_.raster(); }

        inline vectkit::FeatureCollection &vector_data() { return poly_data_.collection(); }
        inline rastkit::RasterCollection &raster_data() { return grid_data_.raster(); }

        inline std::string global_property(const char *global_name) const {
            auto field_props = poly_data_.global_properties();
            auto it = field_props.find(global_name);
            if (it != field_props.end()) {
                return it->second;
            }

            if (grid_data_.has_layers()) {
                auto metadata = grid_data_.raster().getGlobalPropertiesFromFirstLayer();
                auto grid_it = metadata.find(global_name);
                if (grid_it != metadata.end()) {
                    return grid_it->second;
                }
            }
            return "";
        }

        inline void set_global_property(const char *global_name, const std::string &value) {
            poly_data_.set_global_property(global_name, value);
            if (grid_data_.has_layers()) {
                grid_data_.get_layer(0).setGlobalProperty(global_name, value);
            }
        }

        inline void sync_to_poly_grid() {
            poly_data_.set_name(name_);
            poly_data_.set_type(type_);
            poly_data_.set_id(id_);

            grid_data_.set_name(name_);
            grid_data_.set_type(type_);
            grid_data_.set_id(id_);
        }

        // Accessors for internal data structures
        inline Poly &poly() { return poly_data_; }
        inline const Poly &poly() const { return poly_data_; }

        inline Grid &grid() { return grid_data_; }
        inline const Grid &grid() const { return grid_data_; }

        // ============ Raster Layer Convenience Helpers ============

        /// Get number of raster layers
        inline size_t layer_count() const { return grid_data_.layer_count(); }

        /// Check if zone has any raster layers
        inline bool has_layers() const { return grid_data_.has_layers(); }

        /// Get layer by index
        inline rastkit::Layer &layer(size_t index) { return grid_data_.get_layer(index); }
        inline const rastkit::Layer &layer(size_t index) const { return grid_data_.get_layer(index); }

        /// Visit a raster layer's grid with a callable (handles all grid types)
        template <typename F> auto visit_raster(size_t layer_index, F &&func) {
            return std::visit(std::forward<F>(func), grid_data_.get_layer(layer_index).grid);
        }

        template <typename F> auto visit_raster(size_t layer_index, F &&func) const {
            return std::visit(std::forward<F>(func), grid_data_.get_layer(layer_index).grid);
        }

        /// Get raster layer rows
        inline size_t layer_rows(size_t layer_index) const {
            return rastkit::get_grid_dimensions(grid_data_.get_layer(layer_index).grid).first;
        }

        /// Get raster layer cols
        inline size_t layer_cols(size_t layer_index) const {
            return rastkit::get_grid_dimensions(grid_data_.get_layer(layer_index).grid).second;
        }

        /// Get world point for cell in a raster layer
        inline dp::Point layer_point(size_t layer_index, size_t r, size_t c) const {
            return std::visit([r, c](const auto &g) { return g.get_point(r, c); },
                              grid_data_.get_layer(layer_index).grid);
        }

        /// Type-safe raster access - returns dp::Optional if wrong type
        template <typename T> inline dp::Optional<std::reference_wrapper<dp::Grid<T>>> raster_as(size_t layer_index) {
            auto *ptr = grid_data_.get_layer(layer_index).template gridIf<T>();
            if (ptr)
                return std::ref(*ptr);
            return dp::nullopt;
        }

        template <typename T>
        inline dp::Optional<std::reference_wrapper<const dp::Grid<T>>> raster_as(size_t layer_index) const {
            const auto *ptr = grid_data_.get_layer(layer_index).template gridIf<T>();
            if (ptr)
                return std::cref(*ptr);
            return dp::nullopt;
        }
    };

    // Factory helper for creating zones with validation
    inline Zone make_zone(const std::string &name, const std::string &type, const dp::Polygon &boundary,
                          const dp::Geo &datum, double resolution = DEFAULT_RESOLUTION) {
        // Validate inputs
        if (name.empty()) {
            throw std::invalid_argument("Zone name cannot be empty");
        }
        if (type.empty()) {
            throw std::invalid_argument("Zone type cannot be empty");
        }
        if (boundary.vertices.size() < 3) {
            throw std::invalid_argument("Boundary polygon must have at least 3 points");
        }
        if (resolution <= 0.0) {
            throw std::invalid_argument("Resolution must be positive");
        }

        // Create zone with auto-generated grid
        return Zone(name, type, boundary, datum, resolution);
    }

    /**
     * @brief Builder pattern for constructing Zone objects with fluent interface
     */
    class ZoneBuilder {
      private:
        // Required fields
        std::optional<std::string> name_;
        std::optional<std::string> type_;
        std::optional<dp::Polygon> boundary_;
        std::optional<dp::Geo> datum_;

        // Optional fields with defaults
        double resolution_ = 1.0;
        std::optional<dp::Grid<uint8_t>> initial_grid_;

        // Collections
        std::unordered_map<std::string, std::string> properties_;

        struct RasterLayerConfig {
            dp::Grid<uint8_t> grid;
            std::string name;
            std::string type;
            std::unordered_map<std::string, std::string> properties;
            bool poly_cut;
            int layer_index;
        };
        std::vector<RasterLayerConfig> raster_layers_;

        struct PolygonElementConfig {
            dp::Polygon geometry;
            std::string name;
            std::string type;
            std::string subtype;
            std::unordered_map<std::string, std::string> properties;
        };
        std::vector<PolygonElementConfig> polygon_elements_;

      public:
        ZoneBuilder() = default;

        // Required configuration methods
        inline ZoneBuilder &with_name(const std::string &name) {
            name_ = name;
            return *this;
        }

        inline ZoneBuilder &with_type(const std::string &type) {
            type_ = type;
            return *this;
        }

        inline ZoneBuilder &with_boundary(const dp::Polygon &boundary) {
            boundary_ = boundary;
            return *this;
        }

        inline ZoneBuilder &with_datum(const dp::Geo &datum) {
            datum_ = datum;
            return *this;
        }

        // Optional configuration methods
        inline ZoneBuilder &with_resolution(double resolution) {
            resolution_ = resolution;
            return *this;
        }

        inline ZoneBuilder &with_initial_grid(const dp::Grid<uint8_t> &grid) {
            initial_grid_ = grid;
            return *this;
        }

        inline ZoneBuilder &with_property(const std::string &key, const std::string &value) {
            properties_[key] = value;
            return *this;
        }

        inline ZoneBuilder &with_properties(const std::unordered_map<std::string, std::string> &properties) {
            for (const auto &[key, value] : properties) {
                properties_[key] = value;
            }
            return *this;
        }

        // Element configuration methods
        inline ZoneBuilder &with_raster_layer(const dp::Grid<uint8_t> &grid, const std::string &name,
                                              const std::string &type = "",
                                              const std::unordered_map<std::string, std::string> &properties = {},
                                              bool poly_cut = false, int layer_index = -1) {
            RasterLayerConfig config;
            config.grid = grid;
            config.name = name;
            config.type = type;
            config.properties = properties;
            config.poly_cut = poly_cut;
            config.layer_index = layer_index;
            raster_layers_.push_back(config);
            return *this;
        }

        inline ZoneBuilder &with_polygon_element(const dp::Polygon &geometry, const std::string &name,
                                                 const std::string &type = "", const std::string &subtype = "default",
                                                 const std::unordered_map<std::string, std::string> &properties = {}) {
            PolygonElementConfig config;
            config.geometry = geometry;
            config.name = name;
            config.type = type;
            config.subtype = subtype;
            config.properties = properties;
            polygon_elements_.push_back(config);
            return *this;
        }

        // Validation
        inline bool is_valid() const { return validation_error().empty(); }

        inline std::string validation_error() const {
            if (!name_.has_value() || name_->empty()) {
                return "Zone name is required and cannot be empty";
            }

            if (!type_.has_value() || type_->empty()) {
                return "Zone type is required and cannot be empty";
            }

            if (!boundary_.has_value()) {
                return "Zone boundary is required";
            }

            if (boundary_->vertices.size() < 3) {
                return "Boundary polygon must have at least 3 points";
            }

            if (!datum_.has_value()) {
                return "Zone datum is required";
            }

            if (resolution_ <= 0.0) {
                return "Resolution must be positive (got " + std::to_string(resolution_) + ")";
            }

            return "";
        }

        // Building
        inline Zone build() const {
            // Validate before building
            std::string error = validation_error();
            if (!error.empty()) {
                throw std::invalid_argument("ZoneBuilder validation failed: " + error);
            }

            // Create zone with either initial grid or auto-generated grid
            Zone zone =
                initial_grid_.has_value()
                    ? Zone(name_.value(), type_.value(), boundary_.value(), initial_grid_.value(), datum_.value())
                    : Zone(name_.value(), type_.value(), boundary_.value(), datum_.value(), resolution_);

            // Add properties
            for (const auto &[key, value] : properties_) {
                zone.set_property(key, value);
            }

            // Add raster layers
            for (const auto &layer : raster_layers_) {
                zone.add_raster_layer(layer.grid, layer.name, layer.type, layer.properties, layer.poly_cut,
                                      layer.layer_index);
            }

            // Add polygon elements
            for (const auto &elem : polygon_elements_) {
                zone.add_polygon_element(elem.geometry, elem.name, elem.type, elem.subtype, elem.properties);
            }

            return zone;
        }

        // Reset builder to initial state
        inline void reset() {
            name_.reset();
            type_.reset();
            boundary_.reset();
            datum_.reset();
            resolution_ = 1.0;
            initial_grid_.reset();
            properties_.clear();
            raster_layers_.clear();
            polygon_elements_.clear();
        }
    };

} // namespace zoneout
