#include "zoneout/zoneout/io.hpp"

namespace zoneout {
    namespace io {

        // ========== Zone I/O Functions ==========

        void save_zone(const Zone &zone, const std::filesystem::path &vector_path,
                       const std::filesystem::path &raster_path,
                       const std::optional<std::filesystem::path> &layer_path) {
            zone.to_files(vector_path, raster_path, layer_path);
        }

        Zone load_zone(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path,
                       const std::optional<std::filesystem::path> &layer_path) {
            return Zone::from_files(vector_path, raster_path, layer_path);
        }

        void save_zone(const Zone &zone, const std::filesystem::path &directory) { zone.save(directory); }

        Zone load_zone(const std::filesystem::path &directory) { return Zone::load(directory); }

        // ========== Plot I/O Functions ==========

        void save_plot(const Plot &plot, const std::filesystem::path &directory) { plot.save(directory); }

        Plot load_plot(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                       const concord::Datum &datum) {
            return Plot::load(directory, name, type, datum);
        }

        void save_plot_tar(const Plot &plot, const std::filesystem::path &tar_file) { plot.save_tar(tar_file); }

        Plot load_plot_tar(const std::filesystem::path &tar_file, const std::string &name, const std::string &type,
                           const concord::Datum &datum) {
            return Plot::load_tar(tar_file, name, type, datum);
        }

    } // namespace io
} // namespace zoneout
