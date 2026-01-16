#pragma once

#include "geoson/vector.hpp"
#include "grid.hpp"
#include "poly.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace zoneout {

    inline std::pair<Poly, Grid> loadPolyGrid(const std::filesystem::path &vector_path,
                                              const std::filesystem::path &raster_path) {
        Poly poly;
        Grid grid;

        std::string vector_name, vector_uuid;
        std::string raster_name, raster_uuid;

        if (std::filesystem::exists(vector_path)) {
            poly = Poly::from_file(vector_path);
            vector_name = poly.name();
            vector_uuid = poly.id().toString();
        }

        if (std::filesystem::exists(raster_path)) {
            grid = Grid::from_file(raster_path);
            raster_name = grid.name();
            raster_uuid = grid.id().toString();
        }

        if (!vector_uuid.empty() && !raster_uuid.empty()) {
            if (vector_uuid != raster_uuid) {
                throw std::runtime_error("UUID mismatch between vector (" + vector_uuid + ") and raster (" +
                                         raster_uuid + ") data files");
            }
        }

        if (!vector_name.empty() && !raster_name.empty()) {
            if (vector_name != raster_name) {
                throw std::runtime_error("Name mismatch between vector ('" + vector_name + "') and raster ('" +
                                         raster_name + "') data files");
            }
        }

        return {poly, grid};
    }

    inline void savePolyGrid(const Poly &poly, const Grid &grid, const std::filesystem::path &vector_path,
                             const std::filesystem::path &raster_path, geoson::CRS crs = geoson::CRS::WGS) {
        poly.to_file(vector_path, crs);
        if (grid.has_layers()) {
            grid.to_file(raster_path);
        }
    }

} // namespace zoneout
