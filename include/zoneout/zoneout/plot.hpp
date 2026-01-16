#pragma once

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
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
        inline Plot(const std::string &name, const std::string &type, const dp::Geo &datum)
            : id_(generateUUID()), name_(name), type_(type), datum_(datum) {}

        inline Plot(const UUID &id, const std::string &name, const std::string &type, const dp::Geo &datum)
            : id_(id), name_(name), type_(type), datum_(datum) {}

        inline const UUID &get_id() const { return id_; }

        inline const std::string &get_name() const { return name_; }
        inline void set_name(const std::string &name) { name_ = name; }

        inline const std::string &get_type() const { return type_; }
        inline void set_type(const std::string &type) { type_ = type; }

        inline const dp::Geo &get_datum() const { return datum_; }
        inline void set_datum(const dp::Geo &datum) { datum_ = datum; }

        inline void add_zone(const Zone &zone) { zones_.push_back(zone); }

        inline bool remove_zone(const UUID &zone_id) {
            auto it = std::find_if(zones_.begin(), zones_.end(),
                                   [&zone_id](const Zone &zone) { return zone.id() == zone_id; });
            if (it != zones_.end()) {
                zones_.erase(it);
                return true;
            }
            return false;
        }

        inline Zone *get_zone(const UUID &zone_id) {
            auto it = std::find_if(zones_.begin(), zones_.end(),
                                   [&zone_id](const Zone &zone) { return zone.id() == zone_id; });
            return (it != zones_.end()) ? &(*it) : nullptr;
        }

        inline const Zone *get_zone(const UUID &zone_id) const {
            auto it = std::find_if(zones_.begin(), zones_.end(),
                                   [&zone_id](const Zone &zone) { return zone.id() == zone_id; });
            return (it != zones_.end()) ? &(*it) : nullptr;
        }

        inline const std::vector<Zone> &get_zones() const { return zones_; }
        inline std::vector<Zone> &get_zones() { return zones_; }

        inline size_t get_zone_count() const { return zones_.size(); }

        inline bool empty() const { return zones_.empty(); }

        inline void clear() { zones_.clear(); }

        inline void set_property(const std::string &key, const std::string &value) { properties_[key] = value; }

        inline std::string get_property(const std::string &key) const {
            auto it = properties_.find(key);
            return (it != properties_.end()) ? it->second : "";
        }

        inline const std::unordered_map<std::string, std::string> &get_properties() const { return properties_; }

        inline bool is_valid() const { return !name_.empty() && !type_.empty(); }

        inline void save(const std::filesystem::path &directory) const {
            std::filesystem::create_directories(directory);

            for (size_t i = 0; i < zones_.size(); ++i) {
                auto zone_dir = directory / ("zone_" + std::to_string(i));
                std::filesystem::create_directories(zone_dir);
                auto vector_path = zone_dir / "vector.geojson";
                auto raster_path = zone_dir / "raster.tiff";
                zones_[i].to_files(vector_path, raster_path);
            }
        }

        inline void save_tar(const std::filesystem::path &tar_file) const {
            mtar_t tar;
            int err = mtar_open(&tar, tar_file.string().c_str(), "w");
            if (err != MTAR_ESUCCESS) {
                throw std::runtime_error("Could not create tar file: " + std::string(mtar_strerror(err)));
            }

            auto temp_dir = std::filesystem::temp_directory_path() / ("plot_" + id_.toString());
            save(temp_dir);

            for (const auto &entry : std::filesystem::recursive_directory_iterator(temp_dir)) {
                if (entry.is_regular_file()) {
                    auto relative_path = std::filesystem::relative(entry.path(), temp_dir);
                    std::ifstream file(entry.path(), std::ios::binary);

                    if (file.is_open()) {
                        file.seekg(0, std::ios::end);
                        size_t file_size = file.tellg();
                        file.seekg(0, std::ios::beg);

                        err = mtar_write_file_header(&tar, relative_path.string().c_str(), file_size);
                        if (err != MTAR_ESUCCESS) {
                            file.close();
                            mtar_close(&tar);
                            std::filesystem::remove_all(temp_dir);
                            throw std::runtime_error("Could not write file header: " + std::string(mtar_strerror(err)));
                        }

                        const size_t chunk_size = 8192;
                        std::vector<char> buffer(chunk_size);
                        while (file.read(buffer.data(), chunk_size) || file.gcount() > 0) {
                            err = mtar_write_data(&tar, buffer.data(), file.gcount());
                            if (err != MTAR_ESUCCESS) {
                                file.close();
                                mtar_close(&tar);
                                std::filesystem::remove_all(temp_dir);
                                throw std::runtime_error("Could not write file data: " +
                                                         std::string(mtar_strerror(err)));
                            }
                        }
                        file.close();
                    }
                }
            }

            mtar_finalize(&tar);
            mtar_close(&tar);
            std::filesystem::remove_all(temp_dir);
        }

        inline void to_files(const std::filesystem::path &directory) const { save(directory); }

        inline static Plot load_tar(const std::filesystem::path &tar_file, const std::string &name,
                                    const std::string &type, const dp::Geo &datum) {
            mtar_t tar;
            int err = mtar_open(&tar, tar_file.string().c_str(), "r");
            if (err != MTAR_ESUCCESS) {
                throw std::runtime_error("Could not open tar file: " + std::string(mtar_strerror(err)));
            }

            auto temp_dir = std::filesystem::temp_directory_path() / ("extract_" + std::to_string(std::time(nullptr)));
            std::filesystem::create_directories(temp_dir);

            mtar_header_t header;
            while ((err = mtar_read_header(&tar, &header)) == MTAR_ESUCCESS) {
                auto file_path = temp_dir / header.name;
                std::filesystem::create_directories(file_path.parent_path());

                std::ofstream file(file_path, std::ios::binary);
                if (file.is_open()) {
                    const size_t chunk_size = 8192;
                    std::vector<char> buffer(chunk_size);
                    size_t remaining = header.size;

                    while (remaining > 0) {
                        size_t to_read = std::min(chunk_size, remaining);
                        err = mtar_read_data(&tar, buffer.data(), to_read);
                        if (err != MTAR_ESUCCESS) {
                            file.close();
                            mtar_close(&tar);
                            std::filesystem::remove_all(temp_dir);
                            throw std::runtime_error("Could not read file data: " + std::string(mtar_strerror(err)));
                        }
                        file.write(buffer.data(), to_read);
                        remaining -= to_read;
                    }
                    file.close();
                }

                err = mtar_next(&tar);
                if (err != MTAR_ESUCCESS && err != MTAR_ENULLRECORD) {
                    mtar_close(&tar);
                    std::filesystem::remove_all(temp_dir);
                    throw std::runtime_error("Could not advance to next file: " + std::string(mtar_strerror(err)));
                }
            }

            mtar_close(&tar);

            Plot plot = load(temp_dir, name, type, datum);

            std::filesystem::remove_all(temp_dir);

            return plot;
        }

        inline static Plot load(const std::filesystem::path &directory, const std::string &name,
                                const std::string &type, const dp::Geo &datum = dp::Geo{0.001, 0.001, 1.0}) {
            Plot plot(name, type, datum);
            dp::Geo plot_datum;

            if (std::filesystem::exists(directory)) {
                for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_directory() && entry.path().filename().string().starts_with("zone_")) {
                        try {
                            auto vector_path = entry.path() / "vector.geojson";
                            auto raster_path = entry.path() / "raster.tiff";
                            auto zone = Zone::from_files(vector_path, raster_path);
                            plot_datum = zone.datum();
                            plot.add_zone(zone);
                        } catch (const std::exception &e) {
                            std::cerr << "Warning: Failed to load zone from " << entry.path() << ": " << e.what()
                                      << std::endl;
                        }
                    }
                }
            }
            plot.datum_ = plot_datum;
            return plot;
        }

        inline static Plot from_files(const std::filesystem::path &directory, const std::string &name,
                                      const std::string &type, const dp::Geo &datum) {
            return load(directory, name, type, datum);
        }
    };

    /**
     * @brief Builder pattern for constructing Plot objects with fluent interface
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
        inline PlotBuilder &with_name(const std::string &name) {
            name_ = name;
            return *this;
        }

        inline PlotBuilder &with_type(const std::string &type) {
            type_ = type;
            return *this;
        }

        inline PlotBuilder &with_datum(const dp::Geo &datum) {
            datum_ = datum;
            return *this;
        }

        // Optional configuration methods
        inline PlotBuilder &with_property(const std::string &key, const std::string &value) {
            properties_[key] = value;
            return *this;
        }

        inline PlotBuilder &with_properties(const std::unordered_map<std::string, std::string> &properties) {
            for (const auto &[key, value] : properties) {
                properties_[key] = value;
            }
            return *this;
        }

        // Zone management methods
        inline PlotBuilder &add_zone(const Zone &zone) {
            zones_.push_back(zone);
            return *this;
        }

        inline PlotBuilder &add_zone(Zone &&zone) {
            zones_.push_back(std::move(zone));
            return *this;
        }

        // Inline zone construction using lambda configurator
        inline PlotBuilder &add_zone(std::function<void(ZoneBuilder &)> configurator) {
            zone_configs_.push_back(configurator);
            return *this;
        }

        // Bulk zone addition
        inline PlotBuilder &add_zones(const std::vector<Zone> &zones) {
            for (const auto &zone : zones) {
                zones_.push_back(zone);
            }
            return *this;
        }

        // Validation and building
        inline bool is_valid() const { return validation_error().empty(); }

        inline std::string validation_error() const {
            if (!name_.has_value() || name_->empty()) {
                return "Plot name is required and cannot be empty";
            }

            if (!type_.has_value() || type_->empty()) {
                return "Plot type is required and cannot be empty";
            }

            if (!datum_.has_value()) {
                return "Plot datum is required";
            }

            return "";
        }

        inline Plot build() const {
            // Validate before building
            std::string error = validation_error();
            if (!error.empty()) {
                throw std::invalid_argument("PlotBuilder validation failed: " + error);
            }

            // Create plot
            Plot plot(name_.value(), type_.value(), datum_.value());

            // Add properties
            for (const auto &[key, value] : properties_) {
                plot.set_property(key, value);
            }

            // Add pre-built zones
            for (const auto &zone : zones_) {
                plot.add_zone(zone);
            }

            // Build and add deferred zones from configurators
            for (const auto &config : zone_configs_) {
                ZoneBuilder builder;

                // Use the plot's datum as default if zone doesn't specify one
                builder.with_datum(datum_.value());

                // Apply configuration
                config(builder);

                // Validate and build zone
                if (!builder.is_valid()) {
                    throw std::invalid_argument("Zone configuration invalid in PlotBuilder: " +
                                                builder.validation_error());
                }

                plot.add_zone(builder.build());
            }

            return plot;
        }

        // Reset builder to initial state
        inline void reset() {
            name_.reset();
            type_.reset();
            datum_.reset();
            properties_.clear();
            zones_.clear();
            zone_configs_.clear();
        }

        // Utility: Get current zone count (including pending builders)
        inline size_t zone_count() const { return zones_.size() + zone_configs_.size(); }
    };

} // namespace zoneout
