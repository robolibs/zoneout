#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "concord/geometry/layer/layer.hpp"
#include "geotiv/raster.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    class Layer : public concord::Layer<uint8_t> {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::string subtype_;

      public:
        Layer();

        Layer(const std::string &name, const std::string &type, const std::string &subtype = "default");

        Layer(const std::string &name, const std::string &type, const std::string &subtype, size_t rows, size_t cols,
              size_t layers, double cell_size, double layer_height, const concord::Pose &pose = concord::Pose{},
              bool centered = true, bool reverse_y = false, bool reverse_z = false);

        const UUID &getId() const;
        const std::string &getName() const;
        const std::string &getType() const;
        const std::string &getSubtype() const;

        void setName(const std::string &name);
        void setType(const std::string &type);
        void setSubtype(const std::string &subtype);
        void setId(const UUID &id);

        bool isValid() const;

        void setOcclusion(const concord::Point &world_point, uint8_t occlusion_value);

        uint8_t getOcclusion(const concord::Point &world_point) const;

        void setVolumeOcclusion(const concord::Point &min_point, const concord::Point &max_point,
                                uint8_t occlusion_value);

        bool isPathClear(const concord::Point &start, const concord::Point &end, double robot_height = 2.0,
                         uint8_t threshold = 50, size_t num_samples = 20) const;

        double findSafeHeight(double x, double y, double max_height = 10.0, uint8_t threshold = 50) const;

        void addPolygonOcclusion(const concord::Polygon &obstacle, double min_height, double max_height,
                                 uint8_t occlusion_value = 255);

        void projectGridToLayer(const concord::Grid<uint8_t> &source_grid, size_t target_layer);

        concord::Grid<uint8_t> extractGridFromLayer(size_t layer_index) const;

        std::unordered_map<std::string, std::string> getMetadata() const;

        void setMetadata(const std::unordered_map<std::string, std::string> &metadata);

        static Layer fromFile(const std::filesystem::path &file_path);

        void toFile(const std::filesystem::path &file_path) const;
    };

} // namespace zoneout
