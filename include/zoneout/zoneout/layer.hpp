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

    // Enhanced Layer class - High-level 3D Layer with zone-specific functionality
    class Layer : public concord::Layer<uint8_t> {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::string subtype_;

      public:
        // ========== Constructors ==========
        Layer() : concord::Layer<uint8_t>(), id_(generateUUID()), type_("occlusion"), subtype_("default") {
        }

        Layer(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : concord::Layer<uint8_t>(), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
        }

        Layer(const std::string &name, const std::string &type, const std::string &subtype,
              size_t rows, size_t cols, size_t layers, double cell_size, double layer_height,
              const concord::Pose &pose = concord::Pose{}, bool centered = true,
              bool reverse_y = false, bool reverse_z = false)
            : concord::Layer<uint8_t>(rows, cols, layers, cell_size, layer_height, centered, pose, reverse_y, reverse_z),
              id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
        }

        // ========== Basic Properties ==========
        const UUID &getId() const { return id_; }
        const std::string &getName() const { return name_; }
        const std::string &getType() const { return type_; }
        const std::string &getSubtype() const { return subtype_; }

        void setName(const std::string &name) { name_ = name; }
        void setType(const std::string &type) { type_ = type; }
        void setSubtype(const std::string &subtype) { subtype_ = subtype; }
        void setId(const UUID &id) { id_ = id; }

        // ========== Higher Level Operations ==========
        bool isValid() const { 
            return rows() > 0 && cols() > 0 && layers() > 0 && !name_.empty(); 
        }

        // ========== Robot Navigation Methods ==========
        
        // Set occlusion value at world coordinates
        inline void setOcclusion(const concord::Point &world_point, uint8_t occlusion_value) {
            set_value_at_world(world_point, occlusion_value);
        }
        
        // Get occlusion value at world coordinates  
        inline uint8_t getOcclusion(const concord::Point &world_point) const {
            return get_value_at_world(world_point);
        }
        
        // Set occlusion for a volume (rectangular prism)
        void setVolumeOcclusion(const concord::Point &min_point, const concord::Point &max_point, 
                               uint8_t occlusion_value) {
            // Convert world bounds to grid indices
            auto [min_r, min_c, min_l] = world_to_grid(min_point);
            auto [max_r, max_c, max_l] = world_to_grid(max_point);
            
            // Ensure proper bounds
            size_t start_r = std::min(min_r, max_r);
            size_t end_r = std::max(min_r, max_r);
            size_t start_c = std::min(min_c, max_c);
            size_t end_c = std::max(min_c, max_c);
            size_t start_l = std::min(min_l, max_l);
            size_t end_l = std::max(min_l, max_l);
            
            // Set occlusion values in the volume
            for (size_t r = start_r; r <= end_r && r < rows(); ++r) {
                for (size_t c = start_c; c <= end_c && c < cols(); ++c) {
                    for (size_t l = start_l; l <= end_l && l < layers(); ++l) {
                        (*this)(r, c, l) = occlusion_value;
                    }
                }
            }
        }
        
        // Check if path is clear for robot navigation
        bool isPathClear(const concord::Point &start, const concord::Point &end, 
                        double robot_height = 2.0, uint8_t threshold = 50, size_t num_samples = 20) const {
            // Sample points along the path
            const double dx = (end.x - start.x) / num_samples;
            const double dy = (end.y - start.y) / num_samples;
            const double dz = (end.z - start.z) / num_samples;
            
            for (size_t i = 0; i <= num_samples; ++i) {
                concord::Point sample_point{
                    start.x + i * dx,
                    start.y + i * dy,
                    start.z + i * dz
                };
                
                // Check occlusion from ground to robot height
                concord::Point check_point = sample_point;
                for (double h = 0.0; h <= robot_height; h += layer_height()) {
                    check_point.z = sample_point.z + h;
                    if (getOcclusion(check_point) > threshold) {
                        return false; // Path blocked
                    }
                }
            }
            return true;
        }
        
        // Find safe height for robot at given XY position
        double findSafeHeight(double x, double y, double max_height = 10.0, 
                             uint8_t threshold = 50) const {
            for (double h = 0.0; h <= max_height; h += layer_height()) {
                concord::Point check_point{x, y, h};
                if (getOcclusion(check_point) <= threshold) {
                    return h;
                }
            }
            return max_height; // Return max height if nothing is safe
        }
        
        // Create occlusion from polygon obstacles at specific height range
        void addPolygonOcclusion(const concord::Polygon &obstacle, double min_height, 
                                double max_height, uint8_t occlusion_value = 255) {
            // Find layer indices for height range
            concord::Point min_point{0, 0, min_height};
            concord::Point max_point{0, 0, max_height};
            auto [_, __, min_layer] = world_to_grid(min_point);
            auto [___, ____, max_layer] = world_to_grid(max_point);
            
            // For each layer in the height range
            for (size_t l = min_layer; l <= max_layer && l < layers(); ++l) {
                // For each cell in the layer
                for (size_t r = 0; r < rows(); ++r) {
                    for (size_t c = 0; c < cols(); ++c) {
                        concord::Point cell_center = get_point(r, c, l);
                        if (obstacle.contains(cell_center)) {
                            (*this)(r, c, l) = occlusion_value;
                        }
                    }
                }
            }
        }

        // ========== Integration Methods ==========
        
        // Project 2D grid data to 3D layer at specific height
        void projectGridToLayer(const concord::Grid<uint8_t> &source_grid, size_t target_layer) {
            if (target_layer >= layers()) {
                throw std::out_of_range("Target layer index out of bounds");
            }
            
            size_t proj_rows = std::min(static_cast<size_t>(source_grid.rows()), rows());
            size_t proj_cols = std::min(static_cast<size_t>(source_grid.cols()), cols());
            
            for (size_t r = 0; r < proj_rows; ++r) {
                for (size_t c = 0; c < proj_cols; ++c) {
                    (*this)(r, c, target_layer) = source_grid(r, c);
                }
            }
        }
        
        // Extract 2D grid from specific layer
        concord::Grid<uint8_t> extractGridFromLayer(size_t layer_index) const {
            return extract_grid(layer_index);
        }

        // ========== File I/O ==========
        // Note: Layer data serialization would need custom implementation
        // For now, we provide metadata serialization similar to Grid/Poly
        
        std::unordered_map<std::string, std::string> getMetadata() const {
            return {
                {"uuid", id_.toString()},
                {"name", name_},
                {"type", type_},
                {"subtype", subtype_},
                {"rows", std::to_string(rows())},
                {"cols", std::to_string(cols())},
                {"layers", std::to_string(layers())},
                {"cell_size", std::to_string(inradius())},
                {"layer_height", std::to_string(layer_height())},
                {"centered", centered() ? "true" : "false"},
                {"reverse_y", reverse_y() ? "true" : "false"},
                {"reverse_z", reverse_z() ? "true" : "false"}
            };
        }
        
        void setMetadata(const std::unordered_map<std::string, std::string> &metadata) {
            auto name_it = metadata.find("name");
            if (name_it != metadata.end()) {
                name_ = name_it->second;
            }
            
            auto type_it = metadata.find("type");
            if (type_it != metadata.end()) {
                type_ = type_it->second;
            }
            
            auto subtype_it = metadata.find("subtype");
            if (subtype_it != metadata.end()) {
                subtype_ = subtype_it->second;
            }
            
            auto uuid_it = metadata.find("uuid");
            if (uuid_it != metadata.end()) {
                id_ = UUID(uuid_it->second);
            }
        }

        // ========== File I/O ==========
        // Convert Layer to GeoTIFF using multiple bands (one per Z-layer)
        static Layer fromFile(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                throw std::runtime_error("File does not exist: " + file_path.string());
            }

            // Load raster data using geotiv
            geotiv::Raster raster_data = geotiv::Raster::fromFile(file_path);
            
            if (!raster_data.hasGrids()) {
                throw std::runtime_error("Layer file contains no grid data");
            }

            // Extract layer dimensions and properties from first grid and global properties
            const auto& first_grid = raster_data.getGrid(0).grid;
            auto global_props = raster_data.getGlobalProperties();
            
            // Extract layer configuration from metadata
            std::string name = global_props.count("name") ? global_props.at("name") : "";
            std::string type = global_props.count("type") ? global_props.at("type") : "occlusion";
            std::string subtype = global_props.count("subtype") ? global_props.at("subtype") : "default";
            
            size_t rows = global_props.count("rows") ? std::stoull(global_props.at("rows")) : first_grid.rows();
            size_t cols = global_props.count("cols") ? std::stoull(global_props.at("cols")) : first_grid.cols();
            size_t num_layers = global_props.count("layers") ? std::stoull(global_props.at("layers")) : raster_data.gridCount();
            
            double cell_size = global_props.count("cell_size") ? std::stod(global_props.at("cell_size")) : raster_data.getResolution();
            double layer_height = global_props.count("layer_height") ? std::stod(global_props.at("layer_height")) : 1.0;
            
            bool centered = global_props.count("centered") ? (global_props.at("centered") == "true") : true;
            bool reverse_y = global_props.count("reverse_y") ? (global_props.at("reverse_y") == "true") : false;
            bool reverse_z = global_props.count("reverse_z") ? (global_props.at("reverse_z") == "true") : false;

            // Create Layer instance with extracted parameters
            Layer layer(name, type, subtype, rows, cols, num_layers, cell_size, layer_height,
                       raster_data.getShift(), centered, reverse_y, reverse_z);

            // Load UUID if present
            if (global_props.count("uuid")) {
                layer.id_ = UUID(global_props.at("uuid"));
            }

            // Copy data from each raster layer to corresponding 3D layer
            size_t layers_to_copy = std::min(num_layers, static_cast<size_t>(raster_data.gridCount()));
            for (size_t l = 0; l < layers_to_copy; ++l) {
                const auto& source_grid = raster_data.getGrid(l).grid;
                
                // Copy data ensuring dimensions match
                size_t copy_rows = std::min(rows, static_cast<size_t>(source_grid.rows()));
                size_t copy_cols = std::min(cols, static_cast<size_t>(source_grid.cols()));
                
                for (size_t r = 0; r < copy_rows; ++r) {
                    for (size_t c = 0; c < copy_cols; ++c) {
                        layer(r, c, l) = source_grid(r, c);
                    }
                }
            }

            return layer;
        }

        void toFile(const std::filesystem::path &file_path) const {
            // Create a geotiv::Raster with each Z-layer as a separate GridLayer
            geotiv::Raster raster(
                concord::Datum{0.001, 0.001, 1.0}, // Default datum for layer files
                pose(),                             // Use layer's pose
                inradius()                         // Use layer's cell size as resolution
            );

            // Convert each Z-layer to a 2D grid and add as GridLayer
            for (size_t l = 0; l < layers(); ++l) {
                // Extract 2D grid from this Z-layer
                auto grid_2d = extract_grid(l);
                
                // Create layer name and properties
                std::string layer_name = "layer_" + std::to_string(l);
                std::string layer_type = "height_" + std::to_string(l);
                std::unordered_map<std::string, std::string> layer_props = {
                    {"z_index", std::to_string(l)},
                    {"height_min", std::to_string(l * layer_height())},
                    {"height_max", std::to_string((l + 1) * layer_height())}
                };
                
                // Create GridLayer and add directly to raster's grid_layers_
                geotiv::GridLayer gridLayer(grid_2d, layer_name, layer_type, layer_props);
                
                // Access the raster's protected grid_layers_ - we need a workaround 
                // First create an empty grid with correct dimensions, then replace its data
                raster.addGrid(static_cast<uint32_t>(grid_2d.cols()), 
                              static_cast<uint32_t>(grid_2d.rows()), 
                              layer_name, layer_type, layer_props);
                
                // Now replace the last added grid's data with our extracted grid
                if (raster.hasGrids()) {
                    auto& lastLayer = const_cast<geotiv::GridLayer&>(raster.getGrid(raster.gridCount() - 1));
                    lastLayer.grid = grid_2d;
                }
            }

            // Set global properties with layer metadata
            raster.setGlobalProperty("name", name_);
            raster.setGlobalProperty("type", type_);
            raster.setGlobalProperty("subtype", subtype_);
            raster.setGlobalProperty("uuid", id_.toString());
            raster.setGlobalProperty("rows", std::to_string(rows()));
            raster.setGlobalProperty("cols", std::to_string(cols()));
            raster.setGlobalProperty("layers", std::to_string(layers()));
            raster.setGlobalProperty("cell_size", std::to_string(inradius()));
            raster.setGlobalProperty("layer_height", std::to_string(layer_height()));
            raster.setGlobalProperty("centered", centered() ? "true" : "false");
            raster.setGlobalProperty("reverse_y", reverse_y() ? "true" : "false");
            raster.setGlobalProperty("reverse_z", reverse_z() ? "true" : "false");

            // Save to file using geotiv
            raster.toFile(file_path);
        }
    };

} // namespace zoneout