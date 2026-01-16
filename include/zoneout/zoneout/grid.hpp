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

        inline void sync_to_global_properties() {
            if (has_layers()) {
                std::unordered_map<std::string, std::string> props;
                props["name"] = meta_.name;
                props["type"] = meta_.type;
                props["subtype"] = meta_.subtype;
                props["uuid"] = meta_.id.toString();
                raster_.setGlobalPropertiesOnAllLayers(props);
            }
        }

      public:
        inline Grid() : meta_("", "other", "default"), raster_() {}

        inline Grid(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : meta_(name, type, subtype), raster_() {}

        inline Grid(const std::string &name, const std::string &type, const std::string &subtype, const dp::Geo &datum)
            : meta_(name, type, subtype), raster_() {
            raster_.datum = datum;
        }

        inline Grid(const std::string &name, const std::string &type, const std::string &subtype, const dp::Geo &datum,
                    const dp::Pose &shift, double resolution)
            : meta_(name, type, subtype), raster_() {
            raster_.datum = datum;
            raster_.shift = shift;
            raster_.resolution = resolution;
        }

        inline const UUID &id() const { return meta_.id; }
        inline const std::string &name() const { return meta_.name; }
        inline const std::string &type() const { return meta_.type; }
        inline const std::string &subtype() const { return meta_.subtype; }

        inline void set_name(const std::string &name) {
            meta_.name = name;
            sync_to_global_properties();
        }

        inline void set_type(const std::string &type) {
            meta_.type = type;
            sync_to_global_properties();
        }

        inline void set_subtype(const std::string &subtype) {
            meta_.subtype = subtype;
            sync_to_global_properties();
        }

        inline void set_id(const UUID &id) {
            meta_.id = id;
            sync_to_global_properties();
        }

        inline bool is_valid() const { return has_layers() && !meta_.name.empty(); }

        inline static Grid from_file(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                throw std::runtime_error("File does not exist: " + file_path.string());
            }

            geotiv::RasterCollection raster_data = geotiv::ReadRasterCollection(file_path);

            Grid grid;
            grid.raster_ = std::move(raster_data);

            auto global_props = grid.raster_.getGlobalPropertiesFromFirstLayer();

            auto name_it = global_props.find("name");
            if (name_it != global_props.end()) {
                grid.meta_.name = name_it->second;
            }

            auto type_it = global_props.find("type");
            if (type_it != global_props.end()) {
                grid.meta_.type = type_it->second;
            }

            auto subtype_it = global_props.find("subtype");
            if (subtype_it != global_props.end()) {
                grid.meta_.subtype = subtype_it->second;
            }

            auto uuid_it = global_props.find("uuid");
            if (uuid_it != global_props.end()) {
                grid.meta_.id = UUID(uuid_it->second);
            }

            return grid;
        }

        inline void to_file(const std::filesystem::path &file_path) const {
            const_cast<Grid *>(this)->sync_to_global_properties();
            geotiv::WriteRasterCollection(raster_, file_path);
        }

        inline void add_grid(uint32_t width, uint32_t height, const std::string &name, const std::string &type = "",
                             const std::unordered_map<std::string, std::string> &properties = {}) {
            geotiv::Layer layer;
            layer.width = width;
            layer.height = height;
            layer.samplesPerPixel = 1;
            layer.planarConfig = 1;
            layer.datum = raster_.datum;
            layer.shift = raster_.shift;
            layer.resolution = raster_.resolution;
            // Note: Do NOT set layer.imageDescription here - let geotiv generate the geospatial metadata

            // Create grid with proper dimensions
            layer.grid = dp::make_grid<uint8_t>(height, width, raster_.resolution, true, raster_.shift, uint8_t{0});

            // Store properties as custom tags (including name)
            for (const auto &[key, value] : properties) {
                layer.setGlobalProperty(key, value);
            }
            if (!type.empty()) {
                layer.setGlobalProperty("type", type);
            }
            layer.setGlobalProperty("name", name);

            raster_.layers.push_back(std::move(layer));
            sync_to_global_properties();
        }

        inline void add_grid(const dp::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                             const std::unordered_map<std::string, std::string> &properties = {}) {
            geotiv::Layer layer;
            layer.width = static_cast<uint32_t>(grid.cols);
            layer.height = static_cast<uint32_t>(grid.rows);
            layer.samplesPerPixel = 1;
            layer.planarConfig = 1;
            layer.datum = raster_.datum;
            layer.shift = raster_.shift;
            layer.resolution = raster_.resolution;
            // Note: Do NOT set layer.imageDescription here - let geotiv generate the geospatial metadata
            layer.grid = grid;

            // Store properties as custom tags (including name)
            for (const auto &[key, value] : properties) {
                layer.setGlobalProperty(key, value);
            }
            if (!type.empty()) {
                layer.setGlobalProperty("type", type);
            }
            layer.setGlobalProperty("name", name);

            raster_.layers.push_back(std::move(layer));
            sync_to_global_properties();
        }

        // Accessors for the underlying raster collection
        inline geotiv::RasterCollection &raster() { return raster_; }
        inline const geotiv::RasterCollection &raster() const { return raster_; }

        // Delegation methods for common RasterCollection operations
        inline bool has_layers() const { return !raster_.layers.empty(); }
        inline size_t layer_count() const { return raster_.layers.size(); }
        inline geotiv::Layer &get_layer(size_t index) { return raster_.layers.at(index); }
        inline const geotiv::Layer &get_layer(size_t index) const { return raster_.layers.at(index); }

        inline dp::Geo &datum() { return raster_.datum; }
        inline const dp::Geo &datum() const { return raster_.datum; }
        inline dp::Pose &shift() { return raster_.shift; }
        inline const dp::Pose &shift() const { return raster_.shift; }
        inline double &resolution() { return raster_.resolution; }
        inline double resolution() const { return raster_.resolution; }

        // ============ Layer Removal ============

        /// Remove a layer by index. Returns true if index was valid and layer was removed.
        inline bool remove_layer(size_t index) {
            if (index < raster_.layers.size()) {
                raster_.layers.erase(raster_.layers.begin() + static_cast<std::ptrdiff_t>(index));
                return true;
            }
            return false;
        }

        /// Remove a layer by name. Returns true if found and removed.
        inline bool remove_layer_by_name(const std::string &layer_name) {
            for (size_t i = 0; i < raster_.layers.size(); ++i) {
                auto props = raster_.layers[i].getGlobalProperties();
                auto it = props.find("name");
                if (it != props.end() && it->second == layer_name) {
                    raster_.layers.erase(raster_.layers.begin() + static_cast<std::ptrdiff_t>(i));
                    return true;
                }
            }
            return false;
        }

        /// Clear all layers
        inline void clear_layers() { raster_.layers.clear(); }

        /// Find layer index by name. Returns dp::Optional containing index if found.
        inline dp::Optional<size_t> layer_index_by_name(const std::string &layer_name) const {
            for (size_t i = 0; i < raster_.layers.size(); ++i) {
                auto props = raster_.layers[i].getGlobalProperties();
                auto it = props.find("name");
                if (it != props.end() && it->second == layer_name) {
                    return i;
                }
            }
            return dp::nullopt;
        }

        /// Get layer by name. Returns dp::Optional with reference to layer if found.
        inline dp::Optional<std::reference_wrapper<geotiv::Layer>> layer_by_name(const std::string &layer_name) {
            auto idx = layer_index_by_name(layer_name);
            if (idx.has_value()) {
                return std::ref(raster_.layers[*idx]);
            }
            return dp::nullopt;
        }

        inline dp::Optional<std::reference_wrapper<const geotiv::Layer>>
        layer_by_name(const std::string &layer_name) const {
            auto idx = layer_index_by_name(layer_name);
            if (idx.has_value()) {
                return std::cref(raster_.layers[*idx]);
            }
            return dp::nullopt;
        }
    };

} // namespace zoneout
