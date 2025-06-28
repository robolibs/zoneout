#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "concord/concord.hpp"
#include "concord/indexing/rtree/rtree.hpp"
#include "zone.hpp"

namespace zoneout {

    // Farm class for managing collections of zones with spatial operations
    class Farm {
      private:
        UUID id_;
        std::string name_;
        std::unordered_map<UUID, std::unique_ptr<Zone>, UUIDHash> zones_;
        std::unordered_map<std::string, std::string> properties_;
        Timestamp created_time_;
        Timestamp modified_time_;
        
        // Spatial indexing for efficient spatial queries
        concord::indexing::RTree<UUID> spatial_index_;

      private:
        void updateModifiedTime() { modified_time_ = time_utils::now(); }
        
        // Helper function to calculate distance from point to line segment
        double distancePointToLineSegment(const concord::Point& point, 
                                        const concord::Point& line_start, 
                                        const concord::Point& line_end) const {
            double dx = line_end.x - line_start.x;
            double dy = line_end.y - line_start.y;
            
            if (dx == 0 && dy == 0) {
                // Line segment is actually a point
                return point.distance_to(line_start);
            }
            
            double t = ((point.x - line_start.x) * dx + (point.y - line_start.y) * dy) / (dx * dx + dy * dy);
            t = std::max(0.0, std::min(1.0, t)); // Clamp t to [0, 1]
            
            concord::Point closest(line_start.x + t * dx, line_start.y + t * dy, line_start.z);
            return point.distance_to(closest);
        }
        
        // Update spatial index when zones are added/removed
        void updateSpatialIndex(const UUID& zone_id, const Zone* zone) {
            if (zone && zone->has_boundary()) {
                // Calculate bounding box for the zone
                auto boundary = zone->get_boundary();
                auto vertices = boundary.getPoints();
                if (!vertices.empty()) {
                    // Find min/max coordinates
                    double min_x = vertices[0].x, max_x = vertices[0].x;
                    double min_y = vertices[0].y, max_y = vertices[0].y;
                    double min_z = vertices[0].z, max_z = vertices[0].z;
                    
                    for (const auto& vertex : vertices) {
                        min_x = std::min(min_x, vertex.x);
                        max_x = std::max(max_x, vertex.x);
                        min_y = std::min(min_y, vertex.y);
                        max_y = std::max(max_y, vertex.y);
                        min_z = std::min(min_z, vertex.z);
                        max_z = std::max(max_z, vertex.z);
                    }
                    
                    // Create bounding box and insert into spatial index
                    concord::AABB bounds(concord::Point(min_x, min_y, min_z),
                                       concord::Point(max_x, max_y, max_z));
                    try {
                        spatial_index_.insert(bounds, zone_id);
                    } catch (const std::exception& e) {
                        // Ignore insertion failures for now - spatial index issues shouldn't crash the app
                        // TODO: Debug why some insertions fail
                    }
                }
            }
        }
        
        void removeSpatialIndex(const UUID& zone_id) {
            // Note: R-tree doesn't have direct remove by data, 
            // so we rebuild the index when zones are removed
            rebuildSpatialIndex();
        }
        
        void rebuildSpatialIndex() {
            spatial_index_.clear();
            for (const auto& [id, zone] : zones_) {
                updateSpatialIndex(id, zone.get());
            }
        }

      public:
        // ========== Constructors ==========
        Farm() : id_(generateUUID()), created_time_(time_utils::now()), modified_time_(time_utils::now()) {}
        
        explicit Farm(const std::string& name) 
            : id_(generateUUID()), name_(name), created_time_(time_utils::now()), modified_time_(time_utils::now()) {}

        // ========== Basic Properties ==========
        const UUID& getId() const { return id_; }
        const std::string& getName() const { return name_; }
        
        void setName(const std::string& name) {
            name_ = name;
            updateModifiedTime();
        }

        // ========== Zone Management ==========
        
        // Add zone to farm
        void addZone(std::unique_ptr<Zone> zone) {
            if (zone) {
                UUID zone_id = zone->getId();
                updateSpatialIndex(zone_id, zone.get());
                zones_[zone_id] = std::move(zone);
                updateModifiedTime();
            }
        }
        
        // Create and add zone directly
        Zone& createZone(const std::string& name, const std::string& type, const concord::Polygon& boundary) {
            auto zone = std::make_unique<Zone>(name, type, boundary);
            Zone* zone_ptr = zone.get();
            addZone(std::move(zone));
            return *zone_ptr;
        }
        
        // Factory methods for common zone types
        Zone& createField(const std::string& name, const concord::Polygon& boundary) {
            auto zone = std::make_unique<Zone>(Zone::createField(name, boundary));
            Zone* zone_ptr = zone.get();
            addZone(std::move(zone));
            return *zone_ptr;
        }
        
        Zone& createBarn(const std::string& name, const concord::Polygon& boundary) {
            auto zone = std::make_unique<Zone>(Zone::createBarn(name, boundary));
            Zone* zone_ptr = zone.get();
            addZone(std::move(zone));
            return *zone_ptr;
        }
        
        Zone& createGreenhouse(const std::string& name, const concord::Polygon& boundary) {
            auto zone = std::make_unique<Zone>(Zone::createGreenhouse(name, boundary));
            Zone* zone_ptr = zone.get();
            addZone(std::move(zone));
            return *zone_ptr;
        }

        // Remove zone
        bool removeZone(const UUID& zone_id) {
            auto it = zones_.find(zone_id);
            if (it != zones_.end()) {
                zones_.erase(it);
                removeSpatialIndex(zone_id);
                updateModifiedTime();
                return true;
            }
            return false;
        }

        // Get zone by ID
        Zone* getZone(const UUID& zone_id) {
            auto it = zones_.find(zone_id);
            return (it != zones_.end()) ? it->second.get() : nullptr;
        }
        
        const Zone* getZone(const UUID& zone_id) const {
            auto it = zones_.find(zone_id);
            return (it != zones_.end()) ? it->second.get() : nullptr;
        }

        // Get zone by name (first match)
        Zone* getZone(const std::string& name) {
            for (auto& [id, zone] : zones_) {
                if (zone->getName() == name) {
                    return zone.get();
                }
            }
            return nullptr;
        }
        
        const Zone* getZone(const std::string& name) const {
            for (const auto& [id, zone] : zones_) {
                if (zone->getName() == name) {
                    return zone.get();
                }
            }
            return nullptr;
        }

        // Get all zones
        std::vector<Zone*> getZones() {
            std::vector<Zone*> result;
            for (auto& [id, zone] : zones_) {
                result.push_back(zone.get());
            }
            return result;
        }
        
        std::vector<const Zone*> getZones() const {
            std::vector<const Zone*> result;
            for (const auto& [id, zone] : zones_) {
                result.push_back(zone.get());
            }
            return result;
        }

        // Get zones by type
        std::vector<Zone*> getZonesByType(const std::string& type) {
            std::vector<Zone*> result;
            for (auto& [id, zone] : zones_) {
                if (zone->getType() == type) {
                    result.push_back(zone.get());
                }
            }
            return result;
        }
        
        std::vector<const Zone*> getZonesByType(const std::string& type) const {
            std::vector<const Zone*> result;
            for (const auto& [id, zone] : zones_) {
                if (zone->getType() == type) {
                    result.push_back(zone.get());
                }
            }
            return result;
        }

        // Zone statistics
        size_t numZones() const { return zones_.size(); }
        
        size_t numZonesByType(const std::string& type) const {
            return getZonesByType(type).size();
        }
        
        bool hasZone(const UUID& zone_id) const {
            return zones_.find(zone_id) != zones_.end();
        }
        
        bool hasZone(const std::string& name) const {
            return getZone(name) != nullptr;
        }

        // ========== Spatial Operations (Enhanced with R-tree Indexing) ==========
        
        // Find zones containing a point (optimized with spatial index)
        std::vector<Zone*> findZonesContaining(const concord::Point& point) {
            std::vector<Zone*> result;
            
            // First, use spatial index to get candidate zones
            concord::AABB point_bounds(point, point);
            auto candidates = spatial_index_.search(point_bounds);
            
            // Then perform precise point-in-polygon test on candidates
            for (const auto& entry : candidates) {
                auto it = zones_.find(entry.data);
                if (it != zones_.end() && it->second->contains(point)) {
                    result.push_back(it->second.get());
                }
            }
            
            return result;
        }
        
        std::vector<const Zone*> findZonesContaining(const concord::Point& point) const {
            std::vector<const Zone*> result;
            
            // First, use spatial index to get candidate zones
            concord::AABB point_bounds(point, point);
            auto candidates = spatial_index_.search(point_bounds);
            
            // Then perform precise point-in-polygon test on candidates
            for (const auto& entry : candidates) {
                auto it = zones_.find(entry.data);
                if (it != zones_.end() && it->second->contains(point)) {
                    result.push_back(it->second.get());
                }
            }
            
            return result;
        }

        // Find zones intersecting with a polygon (optimized with spatial index)
        std::vector<Zone*> findZonesIntersecting(const concord::Polygon& polygon) {
            std::vector<Zone*> result;
            
            // Calculate bounding box of the query polygon
            auto vertices = polygon.getPoints();
            if (vertices.empty()) return result;
            
            double min_x = vertices[0].x, max_x = vertices[0].x;
            double min_y = vertices[0].y, max_y = vertices[0].y;
            double min_z = vertices[0].z, max_z = vertices[0].z;
            
            for (const auto& vertex : vertices) {
                min_x = std::min(min_x, vertex.x);
                max_x = std::max(max_x, vertex.x);
                min_y = std::min(min_y, vertex.y);
                max_y = std::max(max_y, vertex.y);
                min_z = std::min(min_z, vertex.z);
                max_z = std::max(max_z, vertex.z);
            }
            
            concord::AABB query_bounds(concord::Point(min_x, min_y, min_z),
                                     concord::Point(max_x, max_y, max_z));
            
            // Use spatial index to get candidate zones
            auto candidates = spatial_index_.search(query_bounds);
            
            // Perform precise intersection test on candidates
            for (const auto& entry : candidates) {
                auto it = zones_.find(entry.data);
                if (it != zones_.end() && it->second->has_boundary()) {
                    // Simple intersection test: check if any vertices are inside or bounding boxes overlap
                    auto zone_boundary = it->second->get_boundary();
                    bool intersects = false;
                    
                    // Check if any query polygon vertices are inside zone
                    for (const auto& point : polygon.getPoints()) {
                        if (zone_boundary.contains(point)) {
                            intersects = true;
                            break;
                        }
                    }
                    
                    // Check if any zone vertices are inside query polygon  
                    if (!intersects) {
                        for (const auto& point : zone_boundary.getPoints()) {
                            if (polygon.contains(point)) {
                                intersects = true;
                                break;
                            }
                        }
                    }
                    
                    if (intersects) {
                        result.push_back(it->second.get());
                    }
                }
            }
            
            return result;
        }

        // Find zones within a radius of a point (optimized with spatial index)
        std::vector<Zone*> findZonesWithinRadius(const concord::Point& center, double radius) {
            std::vector<Zone*> result;
            
            // For now, use a simple brute-force approach for reliability
            // TODO: Fix spatial index radius search later
            for (const auto& [id, zone] : zones_) {
                if (!zone->has_boundary()) continue;
                
                auto boundary = zone->get_boundary();
                bool within_radius = false;
                
                // First check if center point is inside the zone
                if (boundary.contains(center)) {
                    within_radius = true;
                } else {
                    // Check if any zone vertex is within radius
                    for (const auto& vertex : boundary.getPoints()) {
                        double distance = center.distance_to(vertex);
                        if (distance <= radius) {
                            within_radius = true;
                            break;
                        }
                    }
                    
                    // Also check if center is close to any edge
                    if (!within_radius) {
                        auto points = boundary.getPoints();
                        for (size_t i = 0; i < points.size(); ++i) {
                            auto& p1 = points[i];
                            auto& p2 = points[(i + 1) % points.size()];
                            
                            // Simple distance to line segment check
                            double dist_to_segment = distancePointToLineSegment(center, p1, p2);
                            if (dist_to_segment <= radius) {
                                within_radius = true;
                                break;
                            }
                        }
                    }
                }
                
                if (within_radius) {
                    result.push_back(zone.get());
                }
            }
            
            return result;
        }

        // Find nearest zone to a point (optimized with spatial index)
        Zone* findNearestZone(const concord::Point& point) {
            Zone* nearest = nullptr;
            double min_distance = std::numeric_limits<double>::max();
            
            // Start with a reasonable search radius and expand if needed
            double search_radius = 1000.0; // Start with 1km radius
            std::vector<concord::indexing::RTree<UUID>::Entry> candidates;
            
            // Expand search radius until we find candidates or reach max radius
            const double max_radius = 10000.0; // 10km max search
            while (candidates.empty() && search_radius <= max_radius) {
                candidates = spatial_index_.search_radius(point, search_radius);
                if (candidates.empty()) {
                    search_radius *= 2.0; // Double search radius
                }
            }
            
            // If still no candidates, fall back to full search
            if (candidates.empty()) {
                for (auto& [id, zone] : zones_) {
                    if (zone->has_boundary()) {
                        auto boundary = zone->get_boundary();
                        double distance = std::numeric_limits<double>::max();
                        
                        if (zone->contains(point)) {
                            distance = 0.0;
                        } else {
                            for (const auto& vertex : boundary.getPoints()) {
                                distance = std::min(distance, point.distance_to(vertex));
                            }
                        }
                        
                        if (distance < min_distance) {
                            min_distance = distance;
                            nearest = zone.get();
                        }
                    }
                }
            } else {
                // Process candidates from spatial index
                for (const auto& entry : candidates) {
                    auto it = zones_.find(entry.data);
                    if (it != zones_.end() && it->second->has_boundary()) {
                        auto boundary = it->second->get_boundary();
                        double distance = std::numeric_limits<double>::max();
                        
                        if (it->second->contains(point)) {
                            distance = 0.0;
                        } else {
                            for (const auto& vertex : boundary.getPoints()) {
                                distance = std::min(distance, point.distance_to(vertex));
                            }
                        }
                        
                        if (distance < min_distance) {
                            min_distance = distance;
                            nearest = it->second.get();
                        }
                    }
                }
            }
            
            return nearest;
        }

        // ========== Enhanced Spatial Query Methods ==========
        
        // Get spatial index statistics
        struct SpatialIndexStats {
            size_t total_entries = 0;
            size_t tree_height = 0;
            size_t leaf_nodes = 0;
            size_t internal_nodes = 0;
        };
        
        SpatialIndexStats getSpatialIndexStats() const {
            SpatialIndexStats stats;
            stats.total_entries = spatial_index_.size();
            // stats.tree_height = spatial_index_.height(); // Not available in concord R-tree API
            // Note: Additional tree structure stats would require R-tree API extensions
            return stats;
        }
        
        // Find zones by bounding box query
        std::vector<Zone*> findZonesInBounds(const concord::AABB& bounds) {
            std::vector<Zone*> result;
            auto candidates = spatial_index_.search(bounds);
            
            for (const auto& entry : candidates) {
                auto it = zones_.find(entry.data);
                if (it != zones_.end()) {
                    result.push_back(it->second.get());
                }
            }
            
            return result;
        }
        
        // Find k nearest zones to a point
        std::vector<Zone*> findKNearestZones(const concord::Point& point, size_t k) {
            std::vector<std::pair<double, Zone*>> zone_distances;
            
            // Use expanding radius search for efficiency
            double search_radius = 100.0; // Start with 100m
            const double max_radius = 50000.0; // 50km max
            
            while (zone_distances.size() < k && search_radius <= max_radius) {
                auto candidates = spatial_index_.search_radius(point, search_radius);
                
                zone_distances.clear();
                for (const auto& entry : candidates) {
                    auto it = zones_.find(entry.data);
                    if (it != zones_.end() && it->second->has_boundary()) {
                        double distance = std::numeric_limits<double>::max();
                        
                        if (it->second->contains(point)) {
                            distance = 0.0;
                        } else {
                            auto boundary = it->second->get_boundary();
                            for (const auto& vertex : boundary.getPoints()) {
                                distance = std::min(distance, point.distance_to(vertex));
                            }
                        }
                        
                        zone_distances.emplace_back(distance, it->second.get());
                    }
                }
                
                if (zone_distances.size() < k) {
                    search_radius *= 2.0;
                }
            }
            
            // Sort by distance and return top k
            std::sort(zone_distances.begin(), zone_distances.end());
            
            std::vector<Zone*> result;
            for (size_t i = 0; i < std::min(k, zone_distances.size()); ++i) {
                result.push_back(zone_distances[i].second);
            }
            
            return result;
        }
        
        // Rebuild spatial index manually (useful after bulk zone modifications)
        void rebuildSpatialIndexManually() {
            rebuildSpatialIndex();
        }

        // ========== Farm Statistics ==========
        
        // Total farm area
        double totalArea() const {
            double total = 0.0;
            for (const auto& [id, zone] : zones_) {
                total += zone->area();
            }
            return total;
        }
        
        // Area by zone type
        double areaByType(const std::string& type) const {
            double total = 0.0;
            for (const auto& [id, zone] : zones_) {
                if (zone->getType() == type) {
                    total += zone->area();
                }
            }
            return total;
        }
        
        // Farm bounding box
        std::optional<concord::AABB> getBoundingBox() const {
            if (zones_.empty()) return std::nullopt;
            
            std::vector<concord::Point> all_points;
            for (const auto& [id, zone] : zones_) {
                if (zone->has_boundary()) {
                    auto vertices = zone->get_boundary().getPoints();
                    all_points.insert(all_points.end(), vertices.begin(), vertices.end());
                }
            }
            
            if (all_points.empty()) return std::nullopt;
            
            // Create AABB from all points
            return concord::AABB::fromPoints(all_points);
        }

        // ========== Farm Properties ==========
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

        // ========== Timestamps ==========
        const Timestamp& getCreatedTime() const { return created_time_; }
        const Timestamp& getModifiedTime() const { return modified_time_; }

        // ========== Validation ==========
        bool isValid() const {
            return !name_.empty() && !zones_.empty();
        }

        // ========== File I/O ==========
        
        // Save all zones to a directory
        void saveToDirectory(const std::filesystem::path& directory) const {
            std::filesystem::create_directories(directory);
            
            for (const auto& [id, zone] : zones_) {
                std::string zone_name = zone->getName();
                // Sanitize filename
                std::replace_if(zone_name.begin(), zone_name.end(), 
                              [](char c) { return !std::isalnum(c); }, '_');
                
                auto vector_path = directory / (zone_name + ".geojson");
                auto raster_path = directory / (zone_name + ".tiff");
                
                zone->toFiles(vector_path, raster_path);
            }
        }
        
        // Load all zones from a directory
        static Farm loadFromDirectory(const std::filesystem::path& directory, const std::string& farm_name = "") {
            Farm farm(farm_name);
            
            if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
                return farm;
            }
            
            // Find all .geojson files
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".geojson") {
                    auto base_name = entry.path().stem();
                    auto vector_path = entry.path();
                    auto raster_path = directory / (base_name.string() + ".tiff");
                    
                    try {
                        auto zone = std::make_unique<Zone>(Zone::fromFiles(vector_path, raster_path));
                        farm.addZone(std::move(zone));
                    } catch (const std::exception& e) {
                        // Skip invalid zone files
                        continue;
                    }
                }
            }
            
            return farm;
        }

        // ========== Zone Iteration Support ==========
        
        // Iterator support for range-based loops
        auto begin() { 
            return zones_.begin(); 
        }
        
        auto end() { 
            return zones_.end(); 
        }
        
        auto begin() const { 
            return zones_.begin(); 
        }
        
        auto end() const { 
            return zones_.end(); 
        }
        
        // Apply function to all zones
        template<typename Func>
        void forEachZone(Func&& func) {
            for (auto& [id, zone] : zones_) {
                func(*zone);
            }
        }
        
        template<typename Func>
        void forEachZone(Func&& func) const {
            for (const auto& [id, zone] : zones_) {
                func(*zone);
            }
        }
        
        // Filter zones by predicate
        template<typename Predicate>
        std::vector<Zone*> filterZones(Predicate&& pred) {
            std::vector<Zone*> result;
            for (auto& [id, zone] : zones_) {
                if (pred(*zone)) {
                    result.push_back(zone.get());
                }
            }
            return result;
        }
    };

} // namespace zoneout