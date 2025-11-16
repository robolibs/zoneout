#include "zoneout/zoneout/grid.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace zoneout {

    Grid::Grid() : geotiv::Raster(), meta_("", "other", "default") {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype)
        : geotiv::Raster(), meta_(name, type, subtype) {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype,
               const concord::Datum &datum)
        : geotiv::Raster(datum), meta_(name, type, subtype) {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype,
               const concord::Datum &datum, const concord::Pose &shift, double resolution)
        : geotiv::Raster(datum, shift, resolution), meta_(name, type, subtype) {}

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

    bool Grid::is_valid() const { return hasGrids() && !meta_.name.empty(); }

    Grid Grid::from_file(const std::filesystem::path &file_path) {
        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("File does not exist: " + file_path.string());
        }

        geotiv::Raster raster_data = geotiv::Raster::fromFile(file_path);

        Grid grid;

        static_cast<geotiv::Raster &>(grid) = raster_data;

        auto global_props = raster_data.getGlobalProperties();

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

        geotiv::Raster::toFile(file_path);
    }

    void Grid::add_grid(uint32_t width, uint32_t height, const std::string &name, const std::string &type,
                        const std::unordered_map<std::string, std::string> &properties) {
        geotiv::Raster::addGrid(width, height, name, type, properties);
        sync_to_global_properties();
    }

    void Grid::add_grid(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type,
                        const std::unordered_map<std::string, std::string> &properties) {
        auto props = properties;
        if (!type.empty()) {
            props["type"] = type;
        }

        geotiv::GridLayer gridLayer(grid, name, type, props);

        geotiv::Raster::addGrid(static_cast<uint32_t>(grid.cols()), static_cast<uint32_t>(grid.rows()), name, type,
                                properties);

        if (hasGrids()) {
            auto &lastLayer = const_cast<geotiv::GridLayer &>(getGrid(gridCount() - 1));
            lastLayer.grid = grid;
        }

        sync_to_global_properties();
    }

    void Grid::sync_to_global_properties() {
        if (hasGrids()) {
            setGlobalProperty("name", meta_.name);
            setGlobalProperty("type", meta_.type);
            setGlobalProperty("subtype", meta_.subtype);
            setGlobalProperty("uuid", meta_.id.toString());
        }
    }

} // namespace zoneout
