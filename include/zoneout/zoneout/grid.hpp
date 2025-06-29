#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "geotiv/raster.hpp"
#include "utils/uuid.hpp"

namespace zoneout {

    // Enhanced Grid class - High-level Raster with zone-specific functionality
    class Grid : public geotiv::Raster {
      private:
        UUID id_;
        std::string name_;
        std::string type_;
        std::string subtype_;

      public:
        // ========== Constructors ==========
        Grid() : geotiv::Raster(), id_(generateUUID()), type_("other"), subtype_("default") {
        }

        Grid(const std::string &name, const std::string &type, const std::string &subtype = "default")
            : geotiv::Raster(), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
        }

        Grid(const std::string &name, const std::string &type, const std::string &subtype, const concord::Datum &datum)
            : geotiv::Raster(datum), id_(generateUUID()), name_(name), type_(type), subtype_(subtype) {
        }

        Grid(const std::string &name, const std::string &type, const std::string &subtype, const concord::Datum &datum,
             const concord::Euler &heading, geotiv::CRS crs, double resolution)
            : geotiv::Raster(datum, heading, crs, resolution), id_(generateUUID()), name_(name), type_(type),
              subtype_(subtype) {
        }

        // ========== Basic Properties ==========
        const UUID &getId() const { return id_; }
        const std::string &getName() const { return name_; }
        const std::string &getType() const { return type_; }
        const std::string &getSubtype() const { return subtype_; }

        void setName(const std::string &name) {
            name_ = name;
            syncToGlobalProperties();
        }

        void setType(const std::string &type) {
            type_ = type;
            syncToGlobalProperties();
        }

        void setSubtype(const std::string &subtype) {
            subtype_ = subtype;
            syncToGlobalProperties();
        }

        void setId(const UUID &id) {
            id_ = id;
            syncToGlobalProperties();
        }

        // ========== Higher Level Operations ==========
        bool isValid() const { return hasGrids() && !name_.empty(); }

        // ========== File I/O ==========
        static Grid fromFile(const std::filesystem::path &file_path) {
            if (!std::filesystem::exists(file_path)) {
                throw std::runtime_error("File does not exist: " + file_path.string());
            }

            // Load raster data using parent class
            geotiv::Raster raster_data = geotiv::Raster::fromFile(file_path);

            // Create Grid instance
            Grid grid;

            // Copy raster data
            static_cast<geotiv::Raster &>(grid) = raster_data;

            // Extract metadata from global properties
            auto global_props = raster_data.getGlobalProperties();

            auto name_it = global_props.find("name");
            if (name_it != global_props.end()) {
                grid.name_ = name_it->second;
            }

            auto type_it = global_props.find("type");
            if (type_it != global_props.end()) {
                grid.type_ = type_it->second;
            }

            auto subtype_it = global_props.find("subtype");
            if (subtype_it != global_props.end()) {
                grid.subtype_ = subtype_it->second;
            }

            auto uuid_it = global_props.find("uuid");
            if (uuid_it != global_props.end()) {
                grid.id_ = UUID(uuid_it->second);
            }

            return grid;
        }

        void toFile(const std::filesystem::path &file_path) const {
            // Ensure global properties are synced
            const_cast<Grid *>(this)->syncToGlobalProperties();

            // Use parent class to save
            geotiv::Raster::toFile(file_path);
        }

        // Override addGrid to sync properties after adding grids
        void addGrid(uint32_t width, uint32_t height, const std::string& name, 
                    const std::string& type = "", 
                    const std::unordered_map<std::string, std::string>& properties = {}) {
            geotiv::Raster::addGrid(width, height, name, type, properties);
            syncToGlobalProperties();
        }

      private:
        void syncToGlobalProperties() {
            if (hasGrids()) {
                setGlobalProperty("name", name_);
                setGlobalProperty("type", type_);
                setGlobalProperty("subtype", subtype_);
                setGlobalProperty("uuid", id_.toString());
            }
        }
    };

} // namespace zoneout