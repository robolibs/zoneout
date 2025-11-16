#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "geotiv/raster.hpp"
#include "utils/meta.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    class Grid : public geotiv::Raster {
      private:
        Meta meta_;

      public:
        Grid();
        Grid(const std::string &name, const std::string &type, const std::string &subtype = "default");
        Grid(const std::string &name, const std::string &type, const std::string &subtype, const concord::Datum &datum);
        Grid(const std::string &name, const std::string &type, const std::string &subtype, const concord::Datum &datum,
             const concord::Pose &shift, double resolution);

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

        void add_grid(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                      const std::unordered_map<std::string, std::string> &properties = {});

      private:
        void sync_to_global_properties();
    };

} // namespace zoneout
