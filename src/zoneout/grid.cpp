#include "zoneout/zoneout/grid.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace zoneout {

    Grid::Grid() : geotiv::Raster(), id_(generateUUID()), type_("other"), subtype_("default") {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype)
        : geotiv::Raster(), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype,
               const concord::Datum &datum)
        : geotiv::Raster(datum), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {}

    Grid::Grid(const std::string &name, const std::string &type, const std::string &subtype,
               const concord::Datum &datum, const concord::Pose &shift, double resolution)
        : geotiv::Raster(datum, shift, resolution), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {}

    const UUID &Grid::getId() const { return id_; }
    const std::string &Grid::getName() const { return name_; }
    const std::string &Grid::getType() const { return type_; }
    const std::string &Grid::getSubtype() const { return subtype_; }

    void Grid::setName(const std::string &name) {
        name_ = name;
        syncToGlobalProperties();
    }

    void Grid::setType(const std::string &type) {
        type_ = type;
        syncToGlobalProperties();
    }

    void Grid::setSubtype(const std::string &subtype) {
        subtype_ = subtype;
        syncToGlobalProperties();
    }

    void Grid::setId(const UUID &id) {
        id_ = id;
        syncToGlobalProperties();
    }

    bool Grid::isValid() const { return hasGrids() && !name_.empty(); }

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
            grid.name_ = name_it->second;
        }

        auto type_it = global_props.find("type");
        if (type_it != global_props.end()) {
            grid.type_ = type_it->second;
        }

        auto subtype_it = global_props.find("subtype");
        if (subtype_it != global_props.end()) {
            grid.subtype_ = subtype_it->second;
        }

        auto uuid_it = global_props.find("uuid");
        if (uuid_it != global_props.end()) {
            grid.id_ = UUID(uuid_it->second);
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
            setGlobalProperty("name", name_);
            setGlobalProperty("type", type_);
            setGlobalProperty("subtype", subtype_);
            setGlobalProperty("uuid", id_.toString());
        }
    }

} // namespace zoneout
