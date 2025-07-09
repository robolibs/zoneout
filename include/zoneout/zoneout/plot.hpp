#pragma once

#include <algorithm>
#include <filesystem>
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
        Plot(const std::string &name, const std::string &type, const concord::Datum &datum)
            : id_(generateUUID()), name_(name), type_(type), datum_(datum) {}

        Plot(const UUID &id, const std::string &name, const std::string &type, const concord::Datum &datum)
            : id_(id), name_(name), type_(type), datum_(datum) {}

        const UUID &getId() const { return id_; }

        const std::string &getName() const { return name_; }
        void setName(const std::string &name) { name_ = name; }

        const std::string &getType() const { return type_; }
        void setType(const std::string &type) { type_ = type; }

        const concord::Datum &getDatum() const { return datum_; }
        void setDatum(const concord::Datum &datum) { datum_ = datum; }

        void addZone(const Zone &zone) { zones_.push_back(zone); }

        bool removeZone(const UUID &zone_id) {
            auto it = std::find_if(zones_.begin(), zones_.end(),
                                   [&zone_id](const Zone &zone) { return zone.getId() == zone_id; });
            if (it != zones_.end()) {
                zones_.erase(it);
                return true;
            }
            return false;
        }

        Zone *getZone(const UUID &zone_id) {
            auto it = std::find_if(zones_.begin(), zones_.end(),
                                   [&zone_id](const Zone &zone) { return zone.getId() == zone_id; });
            return (it != zones_.end()) ? &(*it) : nullptr;
        }

        const Zone *getZone(const UUID &zone_id) const {
            auto it = std::find_if(zones_.begin(), zones_.end(),
                                   [&zone_id](const Zone &zone) { return zone.getId() == zone_id; });
            return (it != zones_.end()) ? &(*it) : nullptr;
        }

        const std::vector<Zone> &getZones() const { return zones_; }
        std::vector<Zone> &getZones() { return zones_; }

        size_t getZoneCount() const { return zones_.size(); }

        bool empty() const { return zones_.empty(); }

        void clear() { zones_.clear(); }

        void setProperty(const std::string &key, const std::string &value) { properties_[key] = value; }

        std::string getProperty(const std::string &key) const {
            auto it = properties_.find(key);
            return (it != properties_.end()) ? it->second : "";
        }

        const std::unordered_map<std::string, std::string> &getProperties() const { return properties_; }

        bool is_valid() const { return !name_.empty() && !type_.empty(); }

        void save(const std::filesystem::path &directory) const {
            std::filesystem::create_directories(directory);

            for (size_t i = 0; i < zones_.size(); ++i) {
                auto zone_dir = directory / ("zone_" + std::to_string(i));
                std::filesystem::create_directories(zone_dir);
                auto vector_path = zone_dir / "vector.geojson";
                auto raster_path = zone_dir / "raster.tiff";
                zones_[i].toFiles(vector_path, raster_path);
            }
        }

        void toFiles(const std::filesystem::path &directory) const {
            save(directory);
        }

        static Plot load(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                         const concord::Datum &datum) {
            Plot plot(name, type, datum);

            if (std::filesystem::exists(directory)) {
                for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_directory() && entry.path().filename().string().starts_with("zone_")) {
                        try {
                            auto vector_path = entry.path() / "vector.geojson";
                            auto raster_path = entry.path() / "raster.tiff";
                            auto zone = Zone::fromFiles(vector_path, raster_path);
                            plot.addZone(zone);
                        } catch (const std::exception &) {
                            // Skip invalid zone directories
                        }
                    }
                }
            }

            return plot;
        }

        static Plot fromFiles(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                              const concord::Datum &datum) {
            return load(directory, name, type, datum);
        }
    };

} // namespace zoneout
