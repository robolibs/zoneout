#pragma once

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <concord/concord.hpp>
#include <datapod/datapod.hpp>

#include "microtar/microtar.hpp"
#include "utils/uuid.hpp"
#include "zone.hpp"

namespace dp = datapod;

namespace zoneout {

    class Plot {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::vector<Zone> zones_;
        std::unordered_map<std::string, std::string> properties_;
        dp::Geo datum_;

      public:
        Plot(const std::string &name, const std::string &type, const dp::Geo &datum);
        Plot(const UUID &id, const std::string &name, const std::string &type, const dp::Geo &datum);

        const UUID &get_id() const;
        const std::string &get_name() const;
        void set_name(const std::string &name);
        const std::string &get_type() const;
        void set_type(const std::string &type);
        const dp::Geo &get_datum() const;
        void set_datum(const dp::Geo &datum);

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
                             const dp::Geo &datum);
        static Plot load(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                         const dp::Geo &datum = dp::Geo{0.001, 0.001, 1.0});
        static Plot from_files(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                               const dp::Geo &datum);
    };

    /**
     * @brief Builder pattern for constructing Plot objects with fluent interface
     *
     * PlotBuilder provides a fluent, expressive way to construct complex Plot objects
     * with multiple zones. It supports both adding pre-built zones and constructing
     * zones inline using ZoneBuilder.
     *
     * Example:
     * @code
     * auto plot = PlotBuilder()
     *     .with_name("My Farm")
     *     .with_type("agricultural")
     *     .with_datum({52.0, 5.0, 0.0})
     *     .with_property("farm_type", "research")
     *     .add_zone(zone1)
     *     .add_zone([](ZoneBuilder& builder) {
     *         builder.with_name("field_2")
     *                .with_type("agricultural")
     *                .with_boundary(boundary2)
     *                .with_resolution(0.5);
     *     })
     *     .build();
     * @endcode
     */
    class PlotBuilder {
      private:
        // Required fields
        std::optional<std::string> name_;
        std::optional<std::string> type_;
        std::optional<dp::Geo> datum_;

        // Optional fields
        std::unordered_map<std::string, std::string> properties_;

        // Zones
        std::vector<Zone> zones_;

        // Zone builders for deferred construction
        std::vector<std::function<void(ZoneBuilder &)>> zone_configs_;

      public:
        PlotBuilder() = default;

        // Required configuration methods
        PlotBuilder &with_name(const std::string &name);
        PlotBuilder &with_type(const std::string &type);
        PlotBuilder &with_datum(const dp::Geo &datum);

        // Optional configuration methods
        PlotBuilder &with_property(const std::string &key, const std::string &value);
        PlotBuilder &with_properties(const std::unordered_map<std::string, std::string> &properties);

        // Zone management methods
        PlotBuilder &add_zone(const Zone &zone);
        PlotBuilder &add_zone(Zone &&zone);

        // Inline zone construction using lambda configurator
        PlotBuilder &add_zone(std::function<void(ZoneBuilder &)> configurator);

        // Bulk zone addition
        PlotBuilder &add_zones(const std::vector<Zone> &zones);

        // Validation and building
        bool is_valid() const;
        std::string validation_error() const;
        Plot build() const;

        // Reset builder to initial state
        void reset();

        // Utility: Get current zone count (including pending builders)
        size_t zone_count() const;
    };

} // namespace zoneout
