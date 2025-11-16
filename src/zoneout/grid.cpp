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

    const UUID &Grid::getId() const { return meta_.id; }
    const std::string &Grid::getName() const { return meta_.name; }
    const std::string &Grid::getType() const { return meta_.type; }
    const std::string &Grid::getSubtype() const { return meta_.subtype; }

    void Grid::setName(const std::string &name) {
        meta_.name = name;
        syncToGlobalProperties();
    }

    void Grid::setType(const std::string &type) {
        meta_.type = type;
        syncToGlobalProperties();
    }

    void Grid::setSubtype(const std::string &subtype) {
        meta_.subtype = subtype;
        syncToGlobalProperties();
    }

    void Grid::setId(const UUID &id) {
        meta_.id = id;
        syncToGlobalProperties();
    }

    bool Grid::isValid() const { return hasGrids() && !meta_.name.empty(); }

    Grid Grid::fromFile(const std::filesystem::path &file_path) {
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

    void Grid::toFile(const std::filesystem::path &file_path) const {
        const_cast<Grid *>(this)->syncToGlobalProperties();

        geotiv::Raster::toFile(file_path);
    }

    void Grid::addGrid(uint32_t width, uint32_t height, const std::string &name, const std::string &type,
                       const std::unordered_map<std::string, std::string> &properties) {
        geotiv::Raster::addGrid(width, height, name, type, properties);
        syncToGlobalProperties();
    }

    void Grid::addGrid(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type,
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

        syncToGlobalProperties();
    }

    void Grid::syncToGlobalProperties() {
        if (hasGrids()) {
            setGlobalProperty("name", meta_.name);
            setGlobalProperty("type", meta_.type);
            setGlobalProperty("subtype", meta_.subtype);
            setGlobalProperty("uuid", meta_.id.toString());
        }
    }

} // namespace zoneout
