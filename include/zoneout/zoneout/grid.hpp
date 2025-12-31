#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <concord/concord.hpp>
#include <datapod/datapod.hpp>
#include <geotiv/geotiv.hpp>

#include "utils/meta.hpp"
#include "utils/uuid.hpp"

namespace dp = datapod;

namespace zoneout {

    class Grid {
      private:
        Meta meta_;
        geotiv::RasterCollection raster_;

      public:
        Grid();
        Grid(const std::string &name, const std::string &type, const std::string &subtype = "default");
        Grid(const std::string &name, const std::string &type, const std::string &subtype, const dp::Geo &datum);
        Grid(const std::string &name, const std::string &type, const std::string &subtype, const dp::Geo &datum,
             const dp::Pose &shift, double resolution);

        const UUID &get_id() const;
        const std::string &get_name() const;
        const std::string &get_type() const;
        const std::string &get_subtype() const;

        void set_name(const std::string &name);
        void set_type(const std::string &type);
        void set_subtype(const std::string &subtype);
        void set_id(const UUID &id);

        bool is_valid() const;

        static Grid from_file(const std::filesystem::path &file_path);
        void to_file(const std::filesystem::path &file_path) const;

        void add_grid(uint32_t width, uint32_t height, const std::string &name, const std::string &type = "",
                      const std::unordered_map<std::string, std::string> &properties = {});

        void add_grid(const dp::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                      const std::unordered_map<std::string, std::string> &properties = {});

        // Accessors for the underlying raster collection
        geotiv::RasterCollection &raster() { return raster_; }
        const geotiv::RasterCollection &raster() const { return raster_; }

        // Delegation methods for common RasterCollection operations
        bool has_layers() const { return !raster_.layers.empty(); }
        size_t layer_count() const { return raster_.layers.size(); }
        geotiv::Layer &get_layer(size_t index) { return raster_.layers.at(index); }
        const geotiv::Layer &get_layer(size_t index) const { return raster_.layers.at(index); }

        dp::Geo &datum() { return raster_.datum; }
        const dp::Geo &datum() const { return raster_.datum; }
        dp::Pose &shift() { return raster_.shift; }
        const dp::Pose &shift() const { return raster_.shift; }
        double &resolution() { return raster_.resolution; }
        double resolution() const { return raster_.resolution; }

      private:
        void sync_to_global_properties();
    };

} // namespace zoneout
