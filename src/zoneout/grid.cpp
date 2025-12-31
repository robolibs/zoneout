#include "zoneout/zoneout/grid.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace zoneout {

    Grid::Grid() : meta_("", "other", "default"), raster_() {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype)
        : meta_(name, type, subtype), raster_() {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype, const dp::Geo &datum)
        : meta_(name, type, subtype), raster_() {
        raster_.datum = datum;
    }

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype, const dp::Geo &datum,
               const dp::Pose &shift, double resolution)
        : meta_(name, type, subtype), raster_() {
        raster_.datum = datum;
        raster_.shift = shift;
        raster_.resolution = resolution;
    }

    const UUID &Grid::get_id() const { return meta_.id; }
    const std::string &Grid::get_name() const { return meta_.name; }
    const std::string &Grid::get_type() const { return meta_.type; }
    const std::string &Grid::get_subtype() const { return meta_.subtype; }

    void Grid::set_name(const std::string &name) {
        meta_.name = name;
        sync_to_global_properties();
    }

    void Grid::set_type(const std::string &type) {
        meta_.type = type;
        sync_to_global_properties();
    }

    void Grid::set_subtype(const std::string &subtype) {
        meta_.subtype = subtype;
        sync_to_global_properties();
    }

    void Grid::set_id(const UUID &id) {
        meta_.id = id;
        sync_to_global_properties();
    }

    bool Grid::is_valid() const { return has_layers() && !meta_.name.empty(); }

    Grid Grid::from_file(const std::filesystem::path &file_path) {
        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("File does not exist: " + file_path.string());
        }

        geotiv::RasterCollection raster_data = geotiv::ReadRasterCollection(file_path);

        Grid grid;
        grid.raster_ = std::move(raster_data);

        auto global_props = grid.raster_.getGlobalPropertiesFromFirstLayer();

        auto name_it = global_props.find("name");
        if (name_it != global_props.end()) {
            grid.meta_.name = name_it->second;
        }

        auto type_it = global_props.find("type");
        if (type_it != global_props.end()) {
            grid.meta_.type = type_it->second;
        }

        auto subtype_it = global_props.find("subtype");
        if (subtype_it != global_props.end()) {
            grid.meta_.subtype = subtype_it->second;
        }

        auto uuid_it = global_props.find("uuid");
        if (uuid_it != global_props.end()) {
            grid.meta_.id = UUID(uuid_it->second);
        }

        return grid;
    }

    void Grid::to_file(const std::filesystem::path &file_path) const {
        const_cast<Grid *>(this)->sync_to_global_properties();

        geotiv::WriteRasterCollection(raster_, file_path);
    }

    void Grid::add_grid(uint32_t width, uint32_t height, const std::string &name, const std::string &type,
                        const std::unordered_map<std::string, std::string> &properties) {
        geotiv::Layer layer;
        layer.width = width;
        layer.height = height;
        layer.samplesPerPixel = 1;
        layer.planarConfig = 1;
        layer.datum = raster_.datum;
        layer.shift = raster_.shift;
        layer.resolution = raster_.resolution;
        layer.imageDescription = name;

        // Create grid with proper dimensions
        layer.grid = dp::make_grid<uint8_t>(height, width, raster_.resolution, true, raster_.shift, uint8_t{0});

        // Store properties as custom tags
        for (const auto &[key, value] : properties) {
            layer.setGlobalProperty(key, value);
        }
        if (!type.empty()) {
            layer.setGlobalProperty("type", type);
        }
        layer.setGlobalProperty("name", name);

        raster_.layers.push_back(std::move(layer));
        sync_to_global_properties();
    }

    void Grid::add_grid(const dp::Grid<uint8_t> &grid, const std::string &name, const std::string &type,
                        const std::unordered_map<std::string, std::string> &properties) {
        geotiv::Layer layer;
        layer.width = static_cast<uint32_t>(grid.cols);
        layer.height = static_cast<uint32_t>(grid.rows);
        layer.samplesPerPixel = 1;
        layer.planarConfig = 1;
        layer.datum = raster_.datum;
        layer.shift = raster_.shift;
        layer.resolution = raster_.resolution;
        layer.imageDescription = name;
        layer.grid = grid;

        // Store properties as custom tags
        for (const auto &[key, value] : properties) {
            layer.setGlobalProperty(key, value);
        }
        if (!type.empty()) {
            layer.setGlobalProperty("type", type);
        }
        layer.setGlobalProperty("name", name);

        raster_.layers.push_back(std::move(layer));
        sync_to_global_properties();
    }

    void Grid::sync_to_global_properties() {
        if (has_layers()) {
            std::unordered_map<std::string, std::string> props;
            props["name"] = meta_.name;
            props["type"] = meta_.type;
            props["subtype"] = meta_.subtype;
            props["uuid"] = meta_.id.toString();
            raster_.setGlobalPropertiesOnAllLayers(props);
        }
    }

} // namespace zoneout
