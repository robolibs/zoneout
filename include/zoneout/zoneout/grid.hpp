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

        const UUID &getId() const;
        const std::string &getName() const;
        const std::string &getType() const;
        const std::string &getSubtype() const;

        void setName(const std::string &name);
        void setType(const std::string &type);
        void setSubtype(const std::string &subtype);
        void setId(const UUID &id);

        bool isValid() const;

        static Grid fromFile(const std::filesystem::path &file_path);
        void toFile(const std::filesystem::path &file_path) const;

        void addGrid(uint32_t width, uint32_t height, const std::string &name, const std::string &type = "",
                     const std::unordered_map<std::string, std::string> &properties = {});

        void addGrid(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                     const std::unordered_map<std::string, std::string> &properties = {});

      private:
        void syncToGlobalProperties();
    };

} // namespace zoneout
