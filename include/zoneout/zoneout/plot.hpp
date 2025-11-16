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

        const UUID &get_id() const;
        const std::string &get_name() const;
        void set_name(const std::string &name);
        const std::string &get_type() const;
        void set_type(const std::string &type);
        const concord::Datum &get_datum() const;
        void set_datum(const concord::Datum &datum);

        void add_zone(const Zone &zone);
        bool remove_zone(const UUID &zone_id);
        Zone *get_zone(const UUID &zone_id);
        const Zone *get_zone(const UUID &zone_id) const;
        const std::vector<Zone> &get_zones() const;
        std::vector<Zone> &get_zones();

        size_t get_zone_count() const;
        bool empty() const;
        void clear();

        void set_property(const std::string &key, const std::string &value);
        std::string get_property(const std::string &key) const;
        const std::unordered_map<std::string, std::string> &get_properties() const;

        bool is_valid() const;

        void save(const std::filesystem::path &directory) const;
        void save_tar(const std::filesystem::path &tar_file) const;
        void to_files(const std::filesystem::path &directory) const;

        static Plot load_tar(const std::filesystem::path &tar_file, const std::string &name, const std::string &type,
                             const concord::Datum &datum);
        static Plot load(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                         const concord::Datum &datum = concord::Datum{0.001, 0.001, 1.0});
        static Plot from_files(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                               const concord::Datum &datum);
    };

} // namespace zoneout
