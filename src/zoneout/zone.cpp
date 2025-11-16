#include "zoneout/zoneout/zone.hpp"

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
#include "entropy/generator.hpp"
#include "zoneout/zoneout/polygrid.hpp"
#include "zoneout/zoneout/utils/time.hpp"
#include "zoneout/zoneout/utils/uuid.hpp"

namespace zoneout {

    // ========== Factory Helper ==========

    Zone make_zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
                   const concord::Datum &datum, double resolution) {
        // Validate inputs
        if (name.empty()) {
            throw std::invalid_argument("Zone name cannot be empty");
        }
        if (type.empty()) {
            throw std::invalid_argument("Zone type cannot be empty");
        }
        if (boundary.getPoints().size() < 3) {
            throw std::invalid_argument("Boundary polygon must have at least 3 points");
        }
        if (resolution <= 0.0) {
            throw std::invalid_argument("Resolution must be positive");
        }

        // Create zone with auto-generated grid
        return Zone(name, type, boundary, datum, resolution);
    }


    // ========== Constructors ==========

    Zone::Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
               const concord::Grid<uint8_t> &initial_grid, const concord::Datum &datum)
        : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
          grid_data_(name, type, "default") {
        set_datum(datum);
        auto aabb = boundary.getAABB();
        concord::Pose grid_pose(aabb.center(), concord::Euler{0, 0, 0});
        grid_data_.setShift(grid_pose);
        grid_data_.add_grid(initial_grid, "base_layer", "terrain");
        sync_to_poly_grid();
    }

    Zone::Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
               const concord::Datum &datum, double resolution)
        : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
          grid_data_(name, type, "default") {
        set_datum(datum);

        auto aabb = boundary.getAABB();

        double padding = resolution * 2.0;
        double grid_width = aabb.size().x + padding;
        double grid_height = aabb.size().y + padding;

        size_t grid_rows = static_cast<size_t>(std::ceil(grid_height / resolution));
        size_t grid_cols = static_cast<size_t>(std::ceil(grid_width / resolution));

        grid_rows = std::max(grid_rows, static_cast<size_t>(1));
        grid_cols = std::max(grid_cols, static_cast<size_t>(1));

        concord::Pose grid_pose(aabb.center(), concord::Euler{0, 0, 0});
        concord::Grid<uint8_t> generated_grid(grid_rows, grid_cols, resolution, true, grid_pose, false);

        grid_data_.setShift(grid_pose);

        entropy::NoiseGen noise;
        noise.SetNoiseType(entropy::NoiseGen::NoiseType_OpenSimplex2);
        auto sz = std::max(aabb.size().x, aabb.size().y);
        noise.SetFrequency(sz / 300000.0f);
        noise.SetSeed(std::random_device{}());

        for (size_t r = 0; r < generated_grid.rows(); ++r) {
            for (size_t c = 0; c < generated_grid.cols(); ++c) {
                auto cell_center = generated_grid.get_point(r, c);

                if (boundary.contains(cell_center)) {
                    generated_grid.set_value(r, c, 255);
                } else {
                    generated_grid.set_value(r, c, 0);
                }
            }
        }

        grid_data_.add_grid(generated_grid, "base_layer", "terrain");
        sync_to_poly_grid();
    }

    // ========== Basic Properties ==========

    const UUID &Zone::id() const { return id_; }
    const std::string &Zone::name() const { return name_; }
    const std::string &Zone::type() const { return type_; }

    void Zone::set_name(const std::string &name) {
        name_ = name;
        poly_data_.set_name(name);
        grid_data_.set_name(name);
    }

    void Zone::set_type(const std::string &type) {
        type_ = type;
        poly_data_.set_type(type);
        grid_data_.set_type(type);
    }

    // ========== Zone Properties ==========

    void Zone::set_property(const std::string &key, const std::string &value) { properties_[key] = value; }

    std::string Zone::get_property(const std::string &key, const std::string &default_value) const {
        auto it = properties_.find(key);
        return (it != properties_.end()) ? it->second : default_value;
    }

    const std::unordered_map<std::string, std::string> &Zone::properties() const { return properties_; }

    // ========== Datum Management ==========

    const concord::Datum &Zone::datum() const { return poly_data_.getDatum(); }

    void Zone::set_datum(const concord::Datum &datum) {
        poly_data_.setDatum(datum);
        grid_data_.setDatum(datum);
    }

    // ========== Raster Layer Management ==========

    void Zone::add_raster_layer(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type,
                              const std::unordered_map<std::string, std::string> &properties, bool poly_cut,
                              int layer_index) {
        if (poly_cut && poly_data_.has_field_boundary()) {
            auto modified_grid = grid;

            auto boundary = poly_data_.getFieldBoundary();

            size_t cells_inside = 0;
            size_t total_cells = modified_grid.rows() * modified_grid.cols();

            for (size_t r = 0; r < modified_grid.rows(); ++r) {
                for (size_t c = 0; c < modified_grid.cols(); ++c) {
                    auto cell_center = modified_grid.get_point(r, c);

                    if (boundary.contains(cell_center)) {
                        cells_inside++;
                    } else {
                        modified_grid.set_value(r, c, 0);
                    }
                }
            }

            grid_data_.add_grid(modified_grid, name, type, properties);
        } else {
            grid_data_.add_grid(grid, name, type, properties);
        }
    }

    std::string Zone::raster_info() const {
        if (grid_data_.gridCount() > 0) {
            const auto &first_layer = grid_data_.getGrid(0);
            return "Raster size: " + std::to_string(first_layer.grid.cols()) + "x" +
                   std::to_string(first_layer.grid.rows()) + " (" + std::to_string(grid_data_.gridCount()) + " layers)";
        }
        return "No raster layers";
    }

    // ========== Polygon Feature Management ==========

    void Zone::add_polygon_feature(const concord::Polygon &geometry, const std::string &name, const std::string &type,
                                 const std::string &subtype,
                                 const std::unordered_map<std::string, std::string> &properties) {
        if (poly_data_.has_field_boundary()) {
            auto boundary = poly_data_.getFieldBoundary();

            for (const auto &point : geometry.getPoints()) {
                if (!boundary.contains(point)) {
                    throw std::runtime_error("Polygon feature '" + name +
                                             "' is not valid: points must be inside field boundary");
                }
            }
        }

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> color_dist(50, 200);
        uint8_t polygon_color = static_cast<uint8_t>(color_dist(gen));

        if (grid_data_.gridCount() > 0) {
            auto &base_grid = grid_data_.getGrid(0).grid;
            for (size_t r = 0; r < base_grid.rows(); ++r) {
                for (size_t c = 0; c < base_grid.cols(); ++c) {
                    auto cell_center = base_grid.get_point(r, c);
                    if (geometry.contains(cell_center)) {
                        base_grid.set_value(r, c, polygon_color);
                    }
                }
            }
        }

        UUID feature_id = generateUUID();

        poly_data_.add_polygon_element(feature_id, name, type, subtype, geometry, properties);
    }

    std::string Zone::feature_info() const {
        const auto &polygon_elements = poly_data_.get_polygon_elements();
        const auto &line_elements = poly_data_.get_line_elements();
        const auto &point_elements = poly_data_.get_point_elements();

        size_t total_features = polygon_elements.size() + line_elements.size() + point_elements.size();

        if (total_features > 0) {
            return "Features: " + std::to_string(polygon_elements.size()) + " polygons, " +
                   std::to_string(line_elements.size()) + " lines, " + std::to_string(point_elements.size()) +
                   " points (" + std::to_string(total_features) + " total)";
        }
        return "No features";
    }

    // ========== Validation ==========

    bool Zone::is_valid() const { return poly_data_.is_valid() && grid_data_.is_valid(); }

    // ========== File I/O ==========

    Zone Zone::from_files(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) {
        auto [poly, grid] = loadPolyGrid(vector_path, raster_path);

        auto datum = poly.getDatum();

        concord::Grid<uint8_t> base_grid;
        if (grid.hasGrids()) {
            base_grid = grid.getGrid(0).grid;
        } else {
            concord::Pose shift{concord::Point{0.0, 0.0, 0.0}, concord::Euler{0, 0, 0}};
            base_grid = concord::Grid<uint8_t>(10, 10, 1.0, true, shift);
        }

        concord::Polygon default_boundary;
        Zone zone("", "", default_boundary, base_grid, datum);
        zone.poly_data_ = poly;
        zone.grid_data_ = grid;

        if (poly.get_name().empty() && !grid.get_name().empty()) {
            zone.name_ = grid.get_name();
        } else {
            zone.name_ = poly.get_name();
        }

        if (poly.get_type().empty() && !grid.get_type().empty()) {
            zone.type_ = grid.get_type();
        } else {
            zone.type_ = poly.get_type();
        }

        if (!poly.get_id().isNull()) {
            zone.id_ = poly.get_id();
        } else if (!grid.get_id().isNull()) {
            zone.id_ = grid.get_id();
        }

        if (std::filesystem::exists(vector_path)) {
            auto field_props = poly.getFieldProperties();
            for (const auto &[key, value] : field_props) {
                if (key.substr(0, 5) == "prop_") {
                    zone.set_property(key.substr(5), value);
                }
            }
        }


        zone.sync_to_poly_grid();
        return zone;
    }

    void Zone::to_files(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path) const {
        const_cast<Zone *>(this)->sync_to_poly_grid();

        auto poly_copy = poly_data_;
        auto grid_copy = grid_data_;

        poly_copy.set_id(id_);
        grid_copy.set_id(id_);

        for (const auto &[key, value] : properties_) {
            poly_copy.setFieldProperty("prop_" + key, value);
        }

        savePolyGrid(poly_copy, grid_copy, vector_path, raster_path);

    }

    void Zone::save(const std::filesystem::path &directory) const {
        std::filesystem::create_directories(directory);
        auto vector_path = directory / "vector.geojson";
        auto raster_path = directory / "raster.tiff";
        to_files(vector_path, raster_path);
    }

    Zone Zone::load(const std::filesystem::path &directory) {
        auto vector_path = directory / "vector.geojson";
        auto raster_path = directory / "raster.tiff";
        return from_files(vector_path, raster_path);
    }

    const geoson::Vector &Zone::vector_data() const { return poly_data_; }
    const geotiv::Raster &Zone::raster_data() const { return grid_data_; }

    geoson::Vector &Zone::vector_data() { return poly_data_; }
    geotiv::Raster &Zone::raster_data() { return grid_data_; }

    // ========== Unified Global Property Access ==========

    std::string Zone::global_property(const char *global_name) const {
        auto field_props = poly_data_.getFieldProperties();
        auto it = field_props.find(global_name);
        if (it != field_props.end()) {
            return it->second;
        }

        if (grid_data_.hasGrids()) {
            auto metadata = grid_data_.getGlobalProperties();
            auto grid_it = metadata.find(global_name);
            if (grid_it != metadata.end()) {
                return grid_it->second;
            }
        }
        return "";
    }

    void Zone::set_global_property(const char *global_name, const std::string &value) {
        poly_data_.setFieldProperty(global_name, value);
        if (grid_data_.hasGrids()) {
            grid_data_.setGlobalProperty(global_name, value);
        }
    }

    void Zone::sync_to_poly_grid() {
        poly_data_.set_name(name_);
        poly_data_.set_type(type_);
        poly_data_.set_id(id_);

        grid_data_.set_name(name_);
        grid_data_.set_type(type_);
        grid_data_.set_id(id_);
    }

    // ========== Accessors for Internal Data Structures ==========

    Poly &Zone::poly() { return poly_data_; }
    const Poly &Zone::poly() const { return poly_data_; }

    Grid &Zone::grid() { return grid_data_; }
    const Grid &Zone::grid() const { return grid_data_; }


} // namespace zoneout
