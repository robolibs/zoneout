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
#include "zoneout/zoneout/layer.hpp"
#include "zoneout/zoneout/polygrid.hpp"
#include "zoneout/zoneout/utils/time.hpp"
#include "zoneout/zoneout/utils/uuid.hpp"

namespace zoneout {

    // ========== Constructors ==========

    Zone::Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
               const concord::Grid<uint8_t> &initial_grid, const concord::Datum &datum)
        : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
          grid_data_(name, type, "default") {
        setDatum(datum);
        auto aabb = boundary.getAABB();
        concord::Pose grid_pose(aabb.center(), concord::Euler{0, 0, 0});
        grid_data_.setShift(grid_pose);
        grid_data_.addGrid(initial_grid, "base_layer", "terrain");
        syncToPolyGrid();
    }

    Zone::Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
               const concord::Datum &datum, double resolution)
        : id_(generateUUID()), name_(name), type_(type), poly_data_(name, type, "default", boundary),
          grid_data_(name, type, "default") {
        setDatum(datum);

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

        grid_data_.addGrid(generated_grid, "base_layer", "terrain");
        syncToPolyGrid();
    }

    // ========== Basic Properties ==========

    const UUID &Zone::getId() const { return id_; }
    const std::string &Zone::getName() const { return name_; }
    const std::string &Zone::getType() const { return type_; }

    void Zone::setName(const std::string &name) {
        name_ = name;
        poly_data_.setName(name);
        grid_data_.setName(name);
    }

    void Zone::setType(const std::string &type) {
        type_ = type;
        poly_data_.setType(type);
        grid_data_.setType(type);
    }

    // ========== Zone Properties ==========

    void Zone::setProperty(const std::string &key, const std::string &value) { properties_[key] = value; }

    std::string Zone::getProperty(const std::string &key, const std::string &default_value) const {
        auto it = properties_.find(key);
        return (it != properties_.end()) ? it->second : default_value;
    }

    const std::unordered_map<std::string, std::string> &Zone::getProperties() const { return properties_; }

    // ========== Datum Management ==========

    const concord::Datum &Zone::getDatum() const { return poly_data_.getDatum(); }

    void Zone::setDatum(const concord::Datum &datum) {
        poly_data_.setDatum(datum);
        grid_data_.setDatum(datum);
    }

    // ========== Raster Layer Management ==========

    void Zone::addRasterLayer(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type,
                              const std::unordered_map<std::string, std::string> &properties, bool poly_cut,
                              int layer_index) {
        if (poly_cut && poly_data_.hasFieldBoundary()) {
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

            grid_data_.addGrid(modified_grid, name, type, properties);
        } else {
            grid_data_.addGrid(grid, name, type, properties);
        }
    }

    std::string Zone::getRasterInfo() const {
        if (grid_data_.gridCount() > 0) {
            const auto &first_layer = grid_data_.getGrid(0);
            return "Raster size: " + std::to_string(first_layer.grid.cols()) + "x" +
                   std::to_string(first_layer.grid.rows()) + " (" + std::to_string(grid_data_.gridCount()) + " layers)";
        }
        return "No raster layers";
    }

    // ========== Polygon Feature Management ==========

    void Zone::addPolygonFeature(const concord::Polygon &geometry, const std::string &name, const std::string &type,
                                 const std::string &subtype,
                                 const std::unordered_map<std::string, std::string> &properties) {
        if (poly_data_.hasFieldBoundary()) {
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

        poly_data_.addPolygonElement(feature_id, name, type, subtype, geometry, properties);
    }

    // ========== 3D Layer Management ==========

    void Zone::initializeOcclusionLayer(size_t height_layers, double layer_height, const std::string &name,
                                        const std::string &type, const std::string &subtype) {
        if (!grid_data_.hasGrids()) {
            throw std::runtime_error(
                "Cannot initialize occlusion layer: Zone has no raster data. Create grid data first.");
        }

        const auto &base_grid = grid_data_.getGrid(0).grid;
        size_t rows = base_grid.rows();
        size_t cols = base_grid.cols();
        double cell_size = base_grid.inradius();
        concord::Pose pose = base_grid.pose();

        layer_data_ = Layer(name, type, subtype, rows, cols, height_layers, cell_size, layer_height, pose);

        std::cout << "✓ Initialized 3D layer matching raster: " << rows << "×" << cols << "×" << height_layers
                  << " (cell: " << cell_size << "m, height: " << layer_height << "m)\n";
    }

    void Zone::initializeOcclusionLayerExplicit(size_t rows, size_t cols, size_t height_layers, double cell_size,
                                                double layer_height, const std::string &name, const std::string &type,
                                                const std::string &subtype, const concord::Pose &pose) {
        layer_data_ = Layer(name, type, subtype, rows, cols, height_layers, cell_size, layer_height, pose);
    }

    bool Zone::hasOcclusionLayer() const { return layer_data_.has_value(); }

    Layer &Zone::getOcclusionLayer() {
        if (!layer_data_.has_value()) {
            throw std::runtime_error("Occlusion layer not initialized. Call initializeOcclusionLayer() first.");
        }
        return *layer_data_;
    }

    const Layer &Zone::getOcclusionLayer() const {
        if (!layer_data_.has_value()) {
            throw std::runtime_error("Occlusion layer not initialized. Call initializeOcclusionLayer() first.");
        }
        return *layer_data_;
    }

    void Zone::setOcclusion(const concord::Point &world_point, uint8_t value) {
        if (hasOcclusionLayer()) {
            layer_data_->setOcclusion(world_point, value);
        }
    }

    uint8_t Zone::getOcclusion(const concord::Point &world_point) const {
        return hasOcclusionLayer() ? layer_data_->getOcclusion(world_point) : 0;
    }

    bool Zone::isPathClear(const concord::Point &start, const concord::Point &end, double robot_height,
                           uint8_t threshold) const {
        return hasOcclusionLayer() ? layer_data_->isPathClear(start, end, robot_height, threshold) : true;
    }

    std::string Zone::getFeatureInfo() const {
        const auto &polygon_elements = poly_data_.getPolygonElements();
        const auto &line_elements = poly_data_.getLineElements();
        const auto &point_elements = poly_data_.getPointElements();

        size_t total_features = polygon_elements.size() + line_elements.size() + point_elements.size();

        if (total_features > 0) {
            return "Features: " + std::to_string(polygon_elements.size()) + " polygons, " +
                   std::to_string(line_elements.size()) + " lines, " + std::to_string(point_elements.size()) +
                   " points (" + std::to_string(total_features) + " total)";
        }
        return "No features";
    }

    // ========== Validation ==========

    bool Zone::is_valid() const {
        bool valid = poly_data_.isValid() && grid_data_.isValid();
        if (hasOcclusionLayer()) {
            valid = valid && layer_data_->isValid();
        }
        return valid;
    }

    // ========== File I/O ==========

    Zone Zone::fromFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path,
                         const std::optional<std::filesystem::path> &layer_path) {
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

        if (poly.getName().empty() && !grid.getName().empty()) {
            zone.name_ = grid.getName();
        } else {
            zone.name_ = poly.getName();
        }

        if (poly.getType().empty() && !grid.getType().empty()) {
            zone.type_ = grid.getType();
        } else {
            zone.type_ = poly.getType();
        }

        if (!poly.getId().isNull()) {
            zone.id_ = poly.getId();
        } else if (!grid.getId().isNull()) {
            zone.id_ = grid.getId();
        }

        if (std::filesystem::exists(vector_path)) {
            auto field_props = poly.getFieldProperties();
            for (const auto &[key, value] : field_props) {
                if (key.substr(0, 5) == "prop_") {
                    zone.setProperty(key.substr(5), value);
                }
            }
        }

        if (layer_path.has_value() && std::filesystem::exists(layer_path.value())) {
            try {
                zone.layer_data_ = Layer::fromFile(layer_path.value());
            } catch (const std::exception &e) {
                std::cerr << "Warning: Failed to load layer data: " << e.what() << std::endl;
            }
        }

        zone.syncToPolyGrid();
        return zone;
    }

    void Zone::toFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path,
                       const std::optional<std::filesystem::path> &layer_path) const {
        const_cast<Zone *>(this)->syncToPolyGrid();

        auto poly_copy = poly_data_;
        auto grid_copy = grid_data_;

        poly_copy.setId(id_);
        grid_copy.setId(id_);

        for (const auto &[key, value] : properties_) {
            poly_copy.setFieldProperty("prop_" + key, value);
        }

        savePolyGrid(poly_copy, grid_copy, vector_path, raster_path);

        if (hasOcclusionLayer() && layer_path.has_value()) {
            try {
                layer_data_->toFile(layer_path.value());
            } catch (const std::exception &e) {
                std::cerr << "Warning: Failed to save layer data: " << e.what() << std::endl;
            }
        }
    }

    void Zone::save(const std::filesystem::path &directory) const {
        std::filesystem::create_directories(directory);
        auto vector_path = directory / "vector.geojson";
        auto raster_path = directory / "raster.tiff";
        auto layer_path = directory / "map.tiff";
        toFiles(vector_path, raster_path, hasOcclusionLayer() ? std::optional(layer_path) : std::nullopt);
    }

    Zone Zone::load(const std::filesystem::path &directory) {
        auto vector_path = directory / "vector.geojson";
        auto raster_path = directory / "raster.tiff";
        auto layer_path = directory / "map.tiff";
        return fromFiles(vector_path, raster_path,
                         std::filesystem::exists(layer_path) ? std::optional(layer_path) : std::nullopt);
    }

    const geoson::Vector &Zone::getVectorData() const { return poly_data_; }
    const geotiv::Raster &Zone::getRasterData() const { return grid_data_; }

    geoson::Vector &Zone::getVectorData() { return poly_data_; }
    geotiv::Raster &Zone::getRasterData() { return grid_data_; }

    // ========== Unified Global Property Access ==========

    std::string Zone::getGlobalProperty(const char *global_name) const {
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

    void Zone::setGlobalProperty(const char *global_name, const std::string &value) {
        poly_data_.setFieldProperty(global_name, value);
        if (grid_data_.hasGrids()) {
            grid_data_.setGlobalProperty(global_name, value);
        }
    }

    void Zone::syncToPolyGrid() {
        poly_data_.setName(name_);
        poly_data_.setType(type_);
        poly_data_.setId(id_);

        grid_data_.setName(name_);
        grid_data_.setType(type_);
        grid_data_.setId(id_);
    }

} // namespace zoneout
