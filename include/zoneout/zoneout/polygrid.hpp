#pragma once

#include "geoson/vector.hpp"
#include "grid.hpp"
#include "poly.hpp"
#include <filesystem>

namespace zoneout {

    std::pair<Poly, Grid> loadPolyGrid(const std::filesystem::path &vector_path,
                                       const std::filesystem::path &raster_path);

    void savePolyGrid(const Poly &poly, const Grid &grid, const std::filesystem::path &vector_path,
                      const std::filesystem::path &raster_path, geoson::CRS crs = geoson::CRS::WGS);

} // namespace zoneout
