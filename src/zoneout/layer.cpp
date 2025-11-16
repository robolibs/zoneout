#include "zoneout/zoneout/layer.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "concord/geometry/layer/layer.hpp"
#include "geotiv/raster.hpp"
#include "zoneout/zoneout/utils/uuid.hpp"

namespace zoneout {

    // ========== Constructors ==========
    Layer::Layer() : concord::Layer<uint8_t>(), meta_("", "occlusion", "default") {}

    Layer::Layer(const std::string &name, const std::string &type, const std::string &subtype)
        : concord::Layer<uint8_t>(), meta_(name, type, subtype) {}

    Layer::Layer(const std::string &name, const std::string &type, const std::string &subtype, size_t rows, size_t cols,
                 size_t layers, double cell_size, double layer_height, const concord::Pose &pose, bool centered,
                 bool reverse_y, bool reverse_z)
        : concord::Layer<uint8_t>(rows, cols, layers, cell_size, layer_height, centered, pose, reverse_y, reverse_z),
          meta_(name, type, subtype) {}

    // ========== Basic Properties ==========
    const UUID &Layer::getId() const { return meta_.id; }
    const std::string &Layer::getName() const { return meta_.name; }
    const std::string &Layer::getType() const { return meta_.type; }
    const std::string &Layer::getSubtype() const { return meta_.subtype; }

    void Layer::setName(const std::string &name) { meta_.name = name; }
    void Layer::setType(const std::string &type) { meta_.type = type; }
    void Layer::setSubtype(const std::string &subtype) { meta_.subtype = subtype; }
    void Layer::setId(const UUID &id) { meta_.id = id; }

    // ========== Higher Level Operations ==========
    bool Layer::isValid() const { return rows() > 0 && cols() > 0 && layers() > 0 && !meta_.name.empty(); }

    // ========== Robot Navigation Methods ==========

    void Layer::setOcclusion(const concord::Point &world_point, uint8_t occlusion_value) {
        set_value_at_world(world_point, occlusion_value);
    }

    uint8_t Layer::getOcclusion(const concord::Point &world_point) const { return get_value_at_world(world_point); }

    void Layer::setVolumeOcclusion(const concord::Point &min_point, const concord::Point &max_point,
                                   uint8_t occlusion_value) {
        auto [min_r, min_c, min_l] = world_to_grid(min_point);
        auto [max_r, max_c, max_l] = world_to_grid(max_point);

        size_t start_r = std::min(min_r, max_r);
        size_t end_r = std::max(min_r, max_r);
        size_t start_c = std::min(min_c, max_c);
        size_t end_c = std::max(min_c, max_c);
        size_t start_l = std::min(min_l, max_l);
        size_t end_l = std::max(min_l, max_l);

        for (size_t r = start_r; r <= end_r && r < rows(); ++r) {
            for (size_t c = start_c; c <= end_c && c < cols(); ++c) {
                for (size_t l = start_l; l <= end_l && l < layers(); ++l) {
                    (*this)(r, c, l) = occlusion_value;
                }
            }
        }
    }

    bool Layer::isPathClear(const concord::Point &start, const concord::Point &end, double robot_height,
                            uint8_t threshold, size_t num_samples) const {
        const double dx = (end.x - start.x) / num_samples;
        const double dy = (end.y - start.y) / num_samples;
        const double dz = (end.z - start.z) / num_samples;

        for (size_t i = 0; i <= num_samples; ++i) {
            concord::Point sample_point{start.x + i * dx, start.y + i * dy, start.z + i * dz};

            concord::Point check_point = sample_point;
            for (double h = 0.0; h <= robot_height; h += layer_height()) {
                check_point.z = sample_point.z + h;
                if (getOcclusion(check_point) > threshold) {
                    return false;
                }
            }
        }
        return true;
    }

    double Layer::findSafeHeight(double x, double y, double max_height, uint8_t threshold) const {
        for (double h = 0.0; h <= max_height; h += layer_height()) {
            concord::Point check_point{x, y, h};
            if (getOcclusion(check_point) <= threshold) {
                return h;
            }
        }
        return max_height;
    }

    void Layer::addPolygonOcclusion(const concord::Polygon &obstacle, double min_height, double max_height,
                                    uint8_t occlusion_value) {
        concord::Point min_point{0, 0, min_height};
        concord::Point max_point{0, 0, max_height};
        auto [_, __, min_layer] = world_to_grid(min_point);
        auto [___, ____, max_layer] = world_to_grid(max_point);

        for (size_t l = min_layer; l <= max_layer && l < layers(); ++l) {
            for (size_t r = 0; r < rows(); ++r) {
                for (size_t c = 0; c < cols(); ++c) {
                    concord::Point cell_center = get_point(r, c, l);
                    if (obstacle.contains(cell_center)) {
                        (*this)(r, c, l) = occlusion_value;
                    }
                }
            }
        }
    }

    // ========== Integration Methods ==========

    void Layer::projectGridToLayer(const concord::Grid<uint8_t> &source_grid, size_t target_layer) {
        if (target_layer >= layers()) {
            throw std::out_of_range("Target layer index out of bounds");
        }

        size_t proj_rows = std::min(static_cast<size_t>(source_grid.rows()), rows());
        size_t proj_cols = std::min(static_cast<size_t>(source_grid.cols()), cols());

        for (size_t r = 0; r < proj_rows; ++r) {
            for (size_t c = 0; c < proj_cols; ++c) {
                (*this)(r, c, target_layer) = source_grid(r, c);
            }
        }
    }

    concord::Grid<uint8_t> Layer::extractGridFromLayer(size_t layer_index) const { return extract_grid(layer_index); }

    // ========== File I/O ==========

    std::unordered_map<std::string, std::string> Layer::getMetadata() const {
        return {{"uuid", meta_.id.toString()},
                {"name", meta_.name},
                {"type", meta_.type},
                {"subtype", meta_.subtype},
                {"rows", std::to_string(rows())},
                {"cols", std::to_string(cols())},
                {"layers", std::to_string(layers())},
                {"cell_size", std::to_string(inradius())},
                {"layer_height", std::to_string(layer_height())},
                {"centered", centered() ? "true" : "false"},
                {"reverse_y", reverse_y() ? "true" : "false"},
                {"reverse_z", reverse_z() ? "true" : "false"}};
    }

    void Layer::setMetadata(const std::unordered_map<std::string, std::string> &metadata) {
        auto name_it = metadata.find("name");
        if (name_it != metadata.end()) {
            meta_.name = name_it->second;
        }

        auto type_it = metadata.find("type");
        if (type_it != metadata.end()) {
            meta_.type = type_it->second;
        }

        auto subtype_it = metadata.find("subtype");
        if (subtype_it != metadata.end()) {
            meta_.subtype = subtype_it->second;
        }

        auto uuid_it = metadata.find("uuid");
        if (uuid_it != metadata.end()) {
            meta_.id = UUID(uuid_it->second);
        }
    }

    Layer Layer::fromFile(const std::filesystem::path &file_path) {
        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("File does not exist: " + file_path.string());
        }

        geotiv::Raster raster_data = geotiv::Raster::fromFile(file_path);

        if (!raster_data.hasGrids()) {
            throw std::runtime_error("Layer file contains no grid data");
        }

        const auto &first_grid = raster_data.getGrid(0).grid;
        auto global_props = raster_data.getGlobalProperties();

        std::string name = global_props.count("name") ? global_props.at("name") : "";
        std::string type = global_props.count("type") ? global_props.at("type") : "occlusion";
        std::string subtype = global_props.count("subtype") ? global_props.at("subtype") : "default";

        size_t rows = global_props.count("rows") ? std::stoull(global_props.at("rows")) : first_grid.rows();
        size_t cols = global_props.count("cols") ? std::stoull(global_props.at("cols")) : first_grid.cols();
        size_t num_layers =
            global_props.count("layers") ? std::stoull(global_props.at("layers")) : raster_data.gridCount();

        double cell_size =
            global_props.count("cell_size") ? std::stod(global_props.at("cell_size")) : raster_data.getResolution();
        double layer_height = global_props.count("layer_height") ? std::stod(global_props.at("layer_height")) : 1.0;

        bool centered = global_props.count("centered") ? (global_props.at("centered") == "true") : true;
        bool reverse_y = global_props.count("reverse_y") ? (global_props.at("reverse_y") == "true") : false;
        bool reverse_z = global_props.count("reverse_z") ? (global_props.at("reverse_z") == "true") : false;

        Layer layer(name, type, subtype, rows, cols, num_layers, cell_size, layer_height, raster_data.getShift(),
                    centered, reverse_y, reverse_z);

        if (global_props.count("uuid")) {
            layer.meta_.id = UUID(global_props.at("uuid"));
        }

        size_t layers_to_copy = std::min(num_layers, static_cast<size_t>(raster_data.gridCount()));
        for (size_t l = 0; l < layers_to_copy; ++l) {
            const auto &source_grid = raster_data.getGrid(l).grid;

            size_t copy_rows = std::min(rows, static_cast<size_t>(source_grid.rows()));
            size_t copy_cols = std::min(cols, static_cast<size_t>(source_grid.cols()));

            for (size_t r = 0; r < copy_rows; ++r) {
                for (size_t c = 0; c < copy_cols; ++c) {
                    layer(r, c, l) = source_grid(r, c);
                }
            }
        }

        return layer;
    }

    void Layer::toFile(const std::filesystem::path &file_path) const {
        geotiv::Raster raster(concord::Datum{0.001, 0.001, 1.0}, pose(), inradius());

        for (size_t l = 0; l < layers(); ++l) {
            auto grid_2d = extract_grid(l);

            std::string layer_name = "layer_" + std::to_string(l);
            std::string layer_type = "height_" + std::to_string(l);
            std::unordered_map<std::string, std::string> layer_props = {
                {"z_index", std::to_string(l)},
                {"height_min", std::to_string(l * layer_height())},
                {"height_max", std::to_string((l + 1) * layer_height())}};

            geotiv::GridLayer gridLayer(grid_2d, layer_name, layer_type, layer_props);

            raster.addGrid(static_cast<uint32_t>(grid_2d.cols()), static_cast<uint32_t>(grid_2d.rows()), layer_name,
                           layer_type, layer_props);

            if (raster.hasGrids()) {
                auto &lastLayer = const_cast<geotiv::GridLayer &>(raster.getGrid(raster.gridCount() - 1));
                lastLayer.grid = grid_2d;
            }
        }

        raster.setGlobalProperty("name", meta_.name);
        raster.setGlobalProperty("type", meta_.type);
        raster.setGlobalProperty("subtype", meta_.subtype);
        raster.setGlobalProperty("uuid", meta_.id.toString());
        raster.setGlobalProperty("rows", std::to_string(rows()));
        raster.setGlobalProperty("cols", std::to_string(cols()));
        raster.setGlobalProperty("layers", std::to_string(layers()));
        raster.setGlobalProperty("cell_size", std::to_string(inradius()));
        raster.setGlobalProperty("layer_height", std::to_string(layer_height()));
        raster.setGlobalProperty("centered", centered() ? "true" : "false");
        raster.setGlobalProperty("reverse_y", reverse_y() ? "true" : "false");
        raster.setGlobalProperty("reverse_z", reverse_z() ? "true" : "false");

        raster.toFile(file_path);
    }

} // namespace zoneout
