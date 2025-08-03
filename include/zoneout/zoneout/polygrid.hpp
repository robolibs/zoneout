#pragma once

// Backward compatibility header - includes both separated files
#include "grid.hpp"
#include "poly.hpp"

namespace zoneout {
    // All Poly and Grid classes are now available through their separate headers
    // Combined File I/O Functions remain here for compatibility

    // Load both Poly and Grid from separate files
    std::pair<Poly, Grid> loadPolyGrid(const std::filesystem::path &vector_path,
                                       const std::filesystem::path &raster_path) {
        Poly poly;
        Grid grid;

        std::string vector_name, vector_uuid;
        std::string raster_name, raster_uuid;

        // Load vector data if exists
        if (std::filesystem::exists(vector_path)) {
            poly = Poly::fromFile(vector_path);
            vector_name = poly.getName();
            vector_uuid = poly.getId().toString();
        }

        // Load raster data if exists
        if (std::filesystem::exists(raster_path)) {
            grid = Grid::fromFile(raster_path);
            raster_name = grid.getName();
            raster_uuid = grid.getId().toString();
        }

        // Validate UUID and NAME consistency
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

    // Save both Poly and Grid to separate files
    void savePolyGrid(const Poly &poly, const Grid &grid, const std::filesystem::path &vector_path,
                      const std::filesystem::path &raster_path, geoson::CRS crs = geoson::CRS::WGS) {
        // TODO: Re-enable validation after debugging
        // Validate consistency before saving
        // if (poly.getId() != grid.getId()) {
        //     throw std::runtime_error("UUID mismatch between Poly and Grid: cannot save inconsistent data");
        // }

        // if (poly.getName() != grid.getName()) {
        //     throw std::runtime_error("Name mismatch between Poly and Grid: cannot save inconsistent data");
        // }

        // Save both files
        poly.toFile(vector_path, crs);
        if (grid.hasGrids()) {
            grid.toFile(raster_path);
        }
    }

} // namespace zoneout
