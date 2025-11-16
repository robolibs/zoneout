#pragma once

#include "plot.hpp"
#include "zone.hpp"
#include <filesystem>
#include <optional>

namespace zoneout {
    namespace io {

        // Zone I/O functions
        void save_zone(const Zone &zone, const std::filesystem::path &vector_path,
                       const std::filesystem::path &raster_path);

        Zone load_zone(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path);

        void save_zone(const Zone &zone, const std::filesystem::path &directory);

        Zone load_zone(const std::filesystem::path &directory);

        // Plot I/O functions
        void save_plot(const Plot &plot, const std::filesystem::path &directory);

        Plot load_plot(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                       const concord::Datum &datum);

        void save_plot_tar(const Plot &plot, const std::filesystem::path &tar_file);

        Plot load_plot_tar(const std::filesystem::path &tar_file, const std::string &name, const std::string &type,
                           const concord::Datum &datum);

    } // namespace io
} // namespace zoneout
