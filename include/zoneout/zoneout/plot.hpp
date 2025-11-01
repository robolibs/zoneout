#pragma once

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "concord/concord.hpp"
#include "microtar/microtar.hpp"
#include "utils/uuid.hpp"
#include "zone.hpp"

namespace zoneout {

    class Plot {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::vector<Zone> zones_;
        std::unordered_map<std::string, std::string> properties_;
        concord::Datum datum_;

      public:
        Plot(const std::string &name, const std::string &type, const concord::Datum &datum);
        Plot(const UUID &id, const std::string &name, const std::string &type, const concord::Datum &datum);

        const UUID &getId() const;
        const std::string &getName() const;
        void setName(const std::string &name);
        const std::string &getType() const;
        void setType(const std::string &type);
        const concord::Datum &getDatum() const;
        void setDatum(const concord::Datum &datum);

        void addZone(const Zone &zone);
        bool removeZone(const UUID &zone_id);
        Zone *getZone(const UUID &zone_id);
        const Zone *getZone(const UUID &zone_id) const;
        const std::vector<Zone> &getZones() const;
        std::vector<Zone> &getZones();

        size_t getZoneCount() const;
        bool empty() const;
        void clear();

        void setProperty(const std::string &key, const std::string &value);
        std::string getProperty(const std::string &key) const;
        const std::unordered_map<std::string, std::string> &getProperties() const;

        bool is_valid() const;

        void save(const std::filesystem::path &directory) const;
        void save_tar(const std::filesystem::path &tar_file) const;
        void toFiles(const std::filesystem::path &directory) const;

        static Plot load_tar(const std::filesystem::path &tar_file, const std::string &name, const std::string &type,
                             const concord::Datum &datum);
        static Plot load(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                         const concord::Datum &datum = concord::Datum{0.001, 0.001, 1.0});
        static Plot fromFiles(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                              const concord::Datum &datum);
    };

} // namespace zoneout
