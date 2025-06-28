#pragma once

#include <algorithm>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "concord/concord.hpp"

#include "uuid_utils.hpp"
#include "time_utils.hpp"

namespace zoneout {

// Zone types based on agricultural use cases
enum class ZoneType {
    Field,
    Barn,
    Shed,
    House,
    Road,
    Storage,
    ProcessingArea,
    SafetyBuffer,
    Other
};

// Subzone types for finer granularity
enum class SubzoneType {
    // Field subzones
    CropRow,
    PlantingSection,
    IrrigationBlock,
    HarvestArea,
    
    // Barn subzones
    CowSpace,
    FeedingArea,
    MilkingStation,
    CleaningZone,
    
    // Road subzones
    Lane,
    Intersection,
    ParkingArea,
    
    // General
    WorkArea,
    RestrictedArea,
    MaintenanceZone,
    Other
};

// Convert enums to strings for serialization
inline std::string zoneTypeToString(ZoneType type) {
    switch (type) {
        case ZoneType::Field: return "field";
        case ZoneType::Barn: return "barn";
        case ZoneType::Shed: return "shed";
        case ZoneType::House: return "house";
        case ZoneType::Road: return "road";
        case ZoneType::Storage: return "storage";
        case ZoneType::ProcessingArea: return "processing_area";
        case ZoneType::SafetyBuffer: return "safety_buffer";
        case ZoneType::Other: return "other";
    }
    return "unknown";
}

inline ZoneType zoneTypeFromString(const std::string& str) {
    if (str == "field") return ZoneType::Field;
    if (str == "barn") return ZoneType::Barn;
    if (str == "shed") return ZoneType::Shed;
    if (str == "house") return ZoneType::House;
    if (str == "road") return ZoneType::Road;
    if (str == "storage") return ZoneType::Storage;
    if (str == "processing_area") return ZoneType::ProcessingArea;
    if (str == "safety_buffer") return ZoneType::SafetyBuffer;
    return ZoneType::Other;
}

inline std::string subzoneTypeToString(SubzoneType type) {
    switch (type) {
        case SubzoneType::CropRow: return "crop_row";
        case SubzoneType::PlantingSection: return "planting_section";
        case SubzoneType::IrrigationBlock: return "irrigation_block";
        case SubzoneType::HarvestArea: return "harvest_area";
        case SubzoneType::CowSpace: return "cow_space";
        case SubzoneType::FeedingArea: return "feeding_area";
        case SubzoneType::MilkingStation: return "milking_station";
        case SubzoneType::CleaningZone: return "cleaning_zone";
        case SubzoneType::Lane: return "lane";
        case SubzoneType::Intersection: return "intersection";
        case SubzoneType::ParkingArea: return "parking_area";
        case SubzoneType::WorkArea: return "work_area";
        case SubzoneType::RestrictedArea: return "restricted_area";
        case SubzoneType::MaintenanceZone: return "maintenance_zone";
        case SubzoneType::Other: return "other";
    }
    return "unknown";
}

// Zone layer structure - uses concord::Grid directly, not geotiv::Layer
struct ZoneLayer {
    std::string name;           // Layer name (e.g., "elevation", "soil_moisture")
    std::string type;           // Layer type (e.g., "height", "moisture", "obstacle")
    concord::Grid<float> grid;  // The actual grid data
    std::string units;          // Units (e.g., "meters", "percentage")
    double resolution;          // Grid resolution in meters
    std::unordered_map<std::string, std::string> metadata;
    
    // Constructor
    ZoneLayer(const std::string& name, const std::string& type, 
              const concord::Grid<float>& grid, double resolution,
              const std::string& units = "")
        : name(name), type(type), grid(grid), units(units), resolution(resolution) {}
    
    // Default constructor
    ZoneLayer() : resolution(1.0) {}
    
    // Get value at a specific point (with interpolation)
    float getValueAt(const concord::Point& point) const {
        // Find the grid cell containing this point
        // This is a simplified implementation - in practice, you'd use proper interpolation
        
        // Get grid bounds from first and last cell
        if (grid.rows() == 0 || grid.cols() == 0) {
            return 0.0f;
        }
        
        auto corners = grid.corners();
        auto min_corner = corners[0]; // top-left
        auto max_corner = corners[2]; // bottom-right
        
        // Calculate grid indices
        double dx = (point.x - min_corner.x) / resolution;
        double dy = (point.y - min_corner.y) / resolution;
        
        size_t row = static_cast<size_t>(std::max(0.0, std::min(dy, static_cast<double>(grid.rows() - 1))));
        size_t col = static_cast<size_t>(std::max(0.0, std::min(dx, static_cast<double>(grid.cols() - 1))));
        
        return grid(row, col).second; // Get the value from the grid cell
    }
    
    // Sample values within a polygon
    std::vector<float> sampleWithin(const concord::Polygon& polygon) const {
        std::vector<float> samples;
        
        if (grid.rows() == 0 || grid.cols() == 0) {
            return samples;
        }
        
        // Get indices of grid cells within the polygon
        auto indices = grid.indices_within(polygon);
        
        for (auto idx : indices) {
            size_t row = idx / grid.cols();
            size_t col = idx % grid.cols();
            samples.push_back(grid(row, col).second);
        }
        
        return samples;
    }
    
    // Get statistics for values within polygon
    struct LayerStats {
        float min_value = 0.0f;
        float max_value = 0.0f;
        float mean_value = 0.0f;
        float std_dev = 0.0f;
        size_t sample_count = 0;
    };
    
    LayerStats getStatsWithin(const concord::Polygon& polygon) const {
        auto samples = sampleWithin(polygon);
        LayerStats stats;
        
        if (samples.empty()) {
            return stats;
        }
        
        stats.sample_count = samples.size();
        stats.min_value = *std::min_element(samples.begin(), samples.end());
        stats.max_value = *std::max_element(samples.begin(), samples.end());
        
        // Calculate mean
        float sum = std::accumulate(samples.begin(), samples.end(), 0.0f);
        stats.mean_value = sum / samples.size();
        
        // Calculate standard deviation
        float variance = 0.0f;
        for (float sample : samples) {
            variance += (sample - stats.mean_value) * (sample - stats.mean_value);
        }
        stats.std_dev = std::sqrt(variance / samples.size());
        
        return stats;
    }
};

// Subzone structure for managing smaller areas within a zone
struct Subzone {
    UUID id;
    std::string name;
    SubzoneType type;
    concord::Polygon boundary;
    std::unordered_map<std::string, std::string> properties;
    Timestamp created_time;
    Timestamp modified_time;
    
    // Constructor
    Subzone(const std::string& name, SubzoneType type, const concord::Polygon& boundary)
        : id(generateUUID()), name(name), type(type), boundary(boundary),
          created_time(time_utils::now()), modified_time(time_utils::now()) {}
    
    // Check if a point is within this subzone
    bool contains(const concord::Point& point) const {
        return boundary.contains(point);
    }
    
    // Get area of the subzone
    double area() const {
        return boundary.area();
    }
    
    // Get perimeter of the subzone
    double perimeter() const {
        return boundary.perimeter();
    }
    
    // Update modified time
    void touch() {
        modified_time = time_utils::now();
    }
    
    // Set property
    void setProperty(const std::string& key, const std::string& value) {
        properties[key] = value;
        touch();
    }
    
    // Get property
    std::string getProperty(const std::string& key, const std::string& default_value = "") const {
        auto it = properties.find(key);
        return (it != properties.end()) ? it->second : default_value;
    }
};

// Main Zone class for managing robotic operation areas
class Zone {
  public:
    // Constructors
    Zone() : id_(generateUUID()), type_(ZoneType::Other), 
             created_time_(time_utils::now()), modified_time_(time_utils::now()) {}
    
    Zone(const std::string& name, ZoneType type)
        : id_(generateUUID()), name_(name), type_(type),
          created_time_(time_utils::now()), modified_time_(time_utils::now()) {}
    
    Zone(const std::string& name, ZoneType type, const concord::Polygon& border)
        : id_(generateUUID()), name_(name), type_(type), border_(border),
          created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

    // Border management
    void setBorder(const concord::Polygon& border) {
        border_ = border;
        updateModifiedTime();
    }
    
    const concord::Polygon& getBorder() const { return border_; }
    
    bool hasBorder() const { return border_.numVertices() >= 3; }
    
    // Layer management
    void addLayer(const ZoneLayer& layer) {
        layers_.push_back(layer);
        updateModifiedTime();
    }
    
    void addLayer(const std::string& name, const std::string& type, 
                  const concord::Grid<float>& grid, double resolution,
                  const std::string& units = "") {
        layers_.emplace_back(name, type, grid, resolution, units);
        updateModifiedTime();
    }
    
    void removeLayer(const std::string& name) {
        layers_.erase(
            std::remove_if(layers_.begin(), layers_.end(),
                          [&](const ZoneLayer& layer) { return layer.name == name; }),
            layers_.end()
        );
        updateModifiedTime();
    }
    
    const std::vector<ZoneLayer>& getLayers() const { return layers_; }
    
    size_t numLayers() const { return layers_.size(); }
    
    // Layer queries
    bool hasLayer(const std::string& name) const {
        return std::find_if(layers_.begin(), layers_.end(),
                           [&](const ZoneLayer& layer) { return layer.name == name; }) != layers_.end();
    }
    
    const ZoneLayer* getLayer(const std::string& name) const {
        auto it = std::find_if(layers_.begin(), layers_.end(),
                              [&](const ZoneLayer& layer) { return layer.name == name; });
        return (it != layers_.end()) ? &(*it) : nullptr;
    }
    
    ZoneLayer* getLayer(const std::string& name) {
        auto it = std::find_if(layers_.begin(), layers_.end(),
                              [&](const ZoneLayer& layer) { return layer.name == name; });
        return (it != layers_.end()) ? &(*it) : nullptr;
    }
    
    const ZoneLayer& getLayerAt(size_t index) const {
        if (index >= layers_.size()) {
            throw std::out_of_range("Layer index out of range");
        }
        return layers_[index];
    }
    
    std::vector<std::string> getLayerNames() const {
        std::vector<std::string> names;
        for (const auto& layer : layers_) {
            names.push_back(layer.name);
        }
        return names;
    }
    
    std::vector<const ZoneLayer*> getLayersByType(const std::string& type) const {
        std::vector<const ZoneLayer*> result;
        for (const auto& layer : layers_) {
            if (layer.type == type) {
                result.push_back(&layer);
            }
        }
        return result;
    }
    
    // Grid queries
    float getValueAt(const std::string& layer_name, const concord::Point& point) const {
        const auto* layer = getLayer(layer_name);
        if (!layer) {
            throw std::invalid_argument("Layer not found: " + layer_name);
        }
        return layer->getValueAt(point);
    }
    
    std::vector<float> sampleGrid(const std::string& layer_name, const concord::Polygon& polygon) const {
        const auto* layer = getLayer(layer_name);
        if (!layer) {
            throw std::invalid_argument("Layer not found: " + layer_name);
        }
        return layer->sampleWithin(polygon);
    }
    
    // Subzone management
    void addSubzone(const std::string& name, const concord::Polygon& boundary, SubzoneType type) {
        subzones_.emplace_back(name, type, boundary);
        updateModifiedTime();
    }
    
    void addSubzone(const Subzone& subzone) {
        subzones_.push_back(subzone);
        updateModifiedTime();
    }
    
    void removeSubzone(const UUID& subzone_id) {
        subzones_.erase(
            std::remove_if(subzones_.begin(), subzones_.end(),
                          [&](const Subzone& s) { return s.id == subzone_id; }),
            subzones_.end()
        );
        updateModifiedTime();
    }
    
    const std::vector<Subzone>& getSubzones() const { return subzones_; }
    
    // Find subzone by ID
    const Subzone* findSubzone(const UUID& id) const {
        auto it = std::find_if(subzones_.begin(), subzones_.end(),
                              [&](const Subzone& s) { return s.id == id; });
        return (it != subzones_.end()) ? &(*it) : nullptr;
    }
    
    Subzone* findSubzone(const UUID& id) {
        auto it = std::find_if(subzones_.begin(), subzones_.end(),
                              [&](const Subzone& s) { return s.id == id; });
        return (it != subzones_.end()) ? &(*it) : nullptr;
    }
    
    // Find subzones containing a point
    std::vector<const Subzone*> findSubzonesContaining(const concord::Point& point) const {
        std::vector<const Subzone*> result;
        for (const auto& subzone : subzones_) {
            if (subzone.contains(point)) {
                result.push_back(&subzone);
            }
        }
        return result;
    }
    
    // Find subzones by type
    std::vector<const Subzone*> findSubzonesByType(SubzoneType type) const {
        std::vector<const Subzone*> result;
        for (const auto& subzone : subzones_) {
            if (subzone.type == type) {
                result.push_back(&subzone);
            }
        }
        return result;
    }
    
    // Spatial queries
    bool contains(const concord::Point& point) const {
        return hasBorder() && border_.contains(point);
    }
    
    bool intersects(const concord::Polygon& polygon) const {
        if (!hasBorder()) return false;
        
        // Simple intersection test - check if any vertices are inside
        for (const auto& point : polygon.getPoints()) {
            if (border_.contains(point)) {
                return true;
            }
        }
        
        // Check if any of our vertices are inside the other polygon
        for (const auto& point : border_.getPoints()) {
            if (polygon.contains(point)) {
                return true;
            }
        }
        
        return false;
    }
    
    double area() const {
        return hasBorder() ? border_.area() : 0.0;
    }
    
    double perimeter() const {
        return hasBorder() ? border_.perimeter() : 0.0;
    }
    
    // Calculate total subzone coverage
    double subzoneCoverage() const {
        double total = 0.0;
        for (const auto& subzone : subzones_) {
            total += subzone.area();
        }
        return total;
    }
    
    // Calculate subzone coverage ratio
    double subzoneCoverageRatio() const {
        double zone_area = area();
        return (zone_area > 0.0) ? (subzoneCoverage() / zone_area) : 0.0;
    }
    
    // Metadata access
    const UUID& getId() const { return id_; }
    const std::string& getName() const { return name_; }
    ZoneType getType() const { return type_; }
    
    void setName(const std::string& name) {
        name_ = name;
        updateModifiedTime();
    }
    
    void setType(ZoneType type) {
        type_ = type;
        updateModifiedTime();
    }
    
    // Property management
    void setProperty(const std::string& key, const std::string& value) {
        properties_[key] = value;
        updateModifiedTime();
    }
    
    std::string getProperty(const std::string& key, const std::string& default_value = "") const {
        auto it = properties_.find(key);
        return (it != properties_.end()) ? it->second : default_value;
    }
    
    const std::unordered_map<std::string, std::string>& getProperties() const {
        return properties_;
    }
    
    // Owner robot tracking
    void setOwnerRobot(const UUID& robot_id) {
        owner_robot_id_ = robot_id;
        updateModifiedTime();
    }
    
    const UUID& getOwnerRobot() const { return owner_robot_id_; }
    
    bool hasOwner() const { return !owner_robot_id_.isNull(); }
    
    // Time tracking
    Timestamp getCreatedTime() const { return created_time_; }
    Timestamp getModifiedTime() const { return modified_time_; }
    
    // Validation
    bool isValid() const {
        return hasBorder() && !layers_.empty() && !name_.empty();
    }
    
    // Create a buffer zone around the current zone
    Zone createBufferZone(double buffer_distance, const std::string& buffer_name_suffix = "_buffer") const {
        Zone buffer_zone(name_ + buffer_name_suffix, ZoneType::SafetyBuffer);
        
        // Copy properties
        buffer_zone.properties_ = properties_;
        std::stringstream ss;
        ss << buffer_distance;
        buffer_zone.setProperty("buffer_distance", ss.str());
        buffer_zone.setProperty("parent_zone", id_.toString());
        
        // TODO: Implement proper polygon buffering using concord algorithms
        // For now, use the same boundary as placeholder
        buffer_zone.border_ = border_;
        
        return buffer_zone;
    }
    
    // Factory methods for common zone types
    static Zone createField(const std::string& name, const concord::Polygon& boundary) {
        Zone field(name, ZoneType::Field, boundary);
        field.setProperty("crop_type", "unknown");
        field.setProperty("planting_date", "");
        field.setProperty("harvest_date", "");
        return field;
    }
    
    static Zone createBarn(const std::string& name, const concord::Polygon& boundary) {
        Zone barn(name, ZoneType::Barn, boundary);
        barn.setProperty("animal_type", "unknown");
        barn.setProperty("capacity", "0");
        barn.setProperty("ventilation", "natural");
        return barn;
    }
    
    static Zone createRoad(const std::string& name, const concord::Polygon& boundary) {
        Zone road(name, ZoneType::Road, boundary);
        road.setProperty("surface_type", "unknown");
        road.setProperty("width", "0");
        road.setProperty("max_speed", "0");
        return road;
    }

  private:
    void updateModifiedTime() {
        modified_time_ = time_utils::now();
    }

  private:
    UUID id_;
    std::string name_;
    ZoneType type_;
    concord::Polygon border_;
    std::vector<ZoneLayer> layers_;
    std::vector<Subzone> subzones_;
    std::unordered_map<std::string, std::string> properties_;
    UUID owner_robot_id_ = UUID::null();
    Timestamp created_time_;
    Timestamp modified_time_;
};

} // namespace zoneout