#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "concord/concord.hpp"
#include "entropy/generator.hpp"
#include "layer.hpp"
#include "polygrid.hpp"
#include "utils/time.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    class Zone {
      private:
        Poly poly_data_;
        Grid grid_data_;
        std::optional<Layer> layer_data_;

        UUID id_;
        std::string name_;
        std::string type_;

        std::unordered_map<std::string, std::string> properties_;

      public:
        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Grid<uint8_t> &initial_grid, const concord::Datum &datum);

        Zone(const std::string &name, const std::string &type, const concord::Polygon &boundary,
             const concord::Datum &datum, double resolution = 1.0);

        const UUID &getId() const;
        const std::string &getName() const;
        const std::string &getType() const;

        void setName(const std::string &name);

        void setType(const std::string &type);

        void setProperty(const std::string &key, const std::string &value);

        std::string getProperty(const std::string &key, const std::string &default_value = "") const;

        const std::unordered_map<std::string, std::string> &getProperties() const;

        const concord::Datum &getDatum() const;

        void setDatum(const concord::Datum &datum);

        void addRasterLayer(const concord::Grid<uint8_t> &grid, const std::string &name, const std::string &type = "",
                            const std::unordered_map<std::string, std::string> &properties = {}, bool poly_cut = false,
                            int layer_index = -1);

        std::string getRasterInfo() const;

        void addPolygonFeature(const concord::Polygon &geometry, const std::string &name, const std::string &type = "",
                               const std::string &subtype = "default",
                               const std::unordered_map<std::string, std::string> &properties = {});

        void initializeOcclusionLayer(size_t height_layers, double layer_height,
                                      const std::string &name = "occlusion_map", const std::string &type = "occlusion",
                                      const std::string &subtype = "robot_navigation");

        void initializeOcclusionLayerExplicit(size_t rows, size_t cols, size_t height_layers, double cell_size,
                                              double layer_height, const std::string &name = "occlusion_map",
                                              const std::string &type = "occlusion",
                                              const std::string &subtype = "robot_navigation",
                                              const concord::Pose &pose = concord::Pose{});

        bool hasOcclusionLayer() const;

        Layer &getOcclusionLayer();

        const Layer &getOcclusionLayer() const;

        void setOcclusion(const concord::Point &world_point, uint8_t value);

        uint8_t getOcclusion(const concord::Point &world_point) const;

        bool isPathClear(const concord::Point &start, const concord::Point &end, double robot_height = 2.0,
                         uint8_t threshold = 50) const;

        std::string getFeatureInfo() const;

        bool is_valid() const;

        static Zone fromFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path,
                              const std::optional<std::filesystem::path> &layer_path = std::nullopt);

        void toFiles(const std::filesystem::path &vector_path, const std::filesystem::path &raster_path,
                     const std::optional<std::filesystem::path> &layer_path = std::nullopt) const;

        void save(const std::filesystem::path &directory) const;

        static Zone load(const std::filesystem::path &directory);

        const geoson::Vector &getVectorData() const;
        const geotiv::Raster &getRasterData() const;

        geoson::Vector &getVectorData();
        geotiv::Raster &getRasterData();

        std::string getGlobalProperty(const char *global_name) const;

        void setGlobalProperty(const char *global_name, const std::string &value);

        void syncToPolyGrid();

        // Accessors for internal data structures
        Poly &poly();
        const Poly &poly() const;

        Grid &grid();
        const Grid &grid() const;

        std::optional<Layer> &occlusion_layer();
        const std::optional<Layer> &occlusion_layer() const;

      private:
    };

} // namespace zoneout
