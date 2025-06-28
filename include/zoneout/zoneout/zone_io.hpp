#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "geoson/geoson.hpp"
#include "geotiv/geotiv.hpp"

#include "zone.hpp"

namespace zoneout {

// Zone I/O error class
class ZoneIOError : public std::runtime_error {
  public:
    explicit ZoneIOError(const std::string& message) : std::runtime_error("Zone I/O Error: " + message) {}
};

namespace zone_io {

// Convert ZoneLayer to geotiv::Layer for saving
geotiv::Layer zoneLayerToGeotiv(const ZoneLayer& zone_layer) {
    geotiv::Layer layer;
    
    // Copy grid dimensions
    layer.width = static_cast<uint32_t>(zone_layer.grid.cols());
    layer.height = static_cast<uint32_t>(zone_layer.grid.rows());
    layer.samplesPerPixel = 1;
    layer.planarConfig = 1;
    
    // Set CRS and resolution
    layer.crs = geotiv::CRS::ENU; // Assuming ENU for now
    layer.resolution = zone_layer.resolution;
    
    // Set image description with metadata
    layer.imageDescription = "Layer: " + zone_layer.name + ", Type: " + zone_layer.type + 
                           ", Units: " + zone_layer.units;
    
    // Copy grid data - need to convert concord::Grid to geotiv format
    // This is a simplified conversion - actual implementation would need proper grid data transfer
    layer.grid = concord::Grid<uint8_t>(zone_layer.grid.rows(), zone_layer.grid.cols(), 
                                       zone_layer.resolution, concord::Datum{}, true, concord::Pose{});
    
    // Convert float values to uint8_t (scaled)
    for (size_t r = 0; r < zone_layer.grid.rows(); ++r) {
        for (size_t c = 0; c < zone_layer.grid.cols(); ++c) {
            float value = zone_layer.grid(r, c).second;
            // Simple scaling to 0-255 range (this should be configurable)
            uint8_t scaled_value = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, value)));
            layer.grid.set_value(r, c, scaled_value);
        }
    }
    
    // Store metadata in custom tags
    for (const auto& [key, value] : zone_layer.metadata) {
        // Store as string in imageDescription for now
        layer.imageDescription += "; " + key + "=" + value;
    }
    
    return layer;
}

// Convert geotiv::Layer back to ZoneLayer
ZoneLayer geotivToZoneLayer(const geotiv::Layer& layer, const std::string& name, const std::string& type) {
    // Create the grid
    concord::Grid<float> grid(layer.height, layer.width, layer.resolution, 
                             concord::Datum{}, true, concord::Pose{});
    
    // Convert uint8_t values back to float
    for (size_t r = 0; r < layer.height; ++r) {
        for (size_t c = 0; c < layer.width; ++c) {
            uint8_t value = layer.grid(r, c).second;
            float float_value = static_cast<float>(value); // Simple conversion
            grid.set_value(r, c, float_value);
        }
    }
    
    ZoneLayer zone_layer(name, type, grid, layer.resolution);
    
    // Parse metadata from imageDescription
    // This is simplified - actual implementation would parse the string properly
    if (layer.imageDescription.find("Units: ") != std::string::npos) {
        size_t start = layer.imageDescription.find("Units: ") + 7;
        size_t end = layer.imageDescription.find(";", start);
        if (end == std::string::npos) end = layer.imageDescription.length();
        zone_layer.units = layer.imageDescription.substr(start, end - start);
    }
    
    return zone_layer;
}

// Convert Zone to geoson::FeatureCollection for saving vector data
geoson::FeatureCollection zoneToGeoJSON(const Zone& zone) {
    geoson::FeatureCollection collection;
    
    // Set datum (simplified)
    collection.datum = concord::Datum{};
    collection.heading = concord::Euler{};
    
    // Main zone feature (border polygon)
    if (zone.hasBorder()) {
        geoson::Feature zone_feature;
        zone_feature.geometry = zone.getBorder();
        
        // Add zone metadata as properties
        zone_feature.properties["id"] = zone.getId().toString();
        zone_feature.properties["name"] = zone.getName();
        zone_feature.properties["type"] = zone.getType();
        zone_feature.properties["feature_type"] = "zone_border";
        zone_feature.properties["created_time"] = time_utils::toISO8601(zone.getCreatedTime());
        zone_feature.properties["modified_time"] = time_utils::toISO8601(zone.getModifiedTime());
        
        if (zone.hasOwner()) {
            zone_feature.properties["owner_robot"] = zone.getOwnerRobot().toString();
        }
        
        // Add all zone properties
        for (const auto& [key, value] : zone.getProperties()) {
            zone_feature.properties["prop_" + key] = value;
        }
        
        // Add layer information
        zone_feature.properties["num_layers"] = std::to_string(zone.numLayers());
        std::string layer_names;
        for (const auto& name : zone.getLayerNames()) {
            if (!layer_names.empty()) layer_names += ",";
            layer_names += name;
        }
        zone_feature.properties["layer_names"] = layer_names;
        
        collection.features.push_back(zone_feature);
    }
    
    // Add subzones as separate features
    for (const auto& subzone : zone.getSubzones()) {
        geoson::Feature subzone_feature;
        subzone_feature.geometry = subzone.boundary;
        
        subzone_feature.properties["id"] = subzone.id.toString();
        subzone_feature.properties["name"] = subzone.name;
        subzone_feature.properties["type"] = subzone.type;
        subzone_feature.properties["feature_type"] = "subzone";
        subzone_feature.properties["parent_zone"] = zone.getId().toString();
        subzone_feature.properties["created_time"] = time_utils::toISO8601(subzone.created_time);
        subzone_feature.properties["modified_time"] = time_utils::toISO8601(subzone.modified_time);
        
        // Add subzone properties
        for (const auto& [key, value] : subzone.properties) {
            subzone_feature.properties["prop_" + key] = value;
        }
        
        collection.features.push_back(subzone_feature);
    }
    
    return collection;
}

// Convert geoson::FeatureCollection back to Zone
Zone geoJSONToZone(const geoson::FeatureCollection& collection) {
    Zone zone;
    std::vector<Subzone> subzones;
    
    for (const auto& feature : collection.features) {
        auto feature_type_it = feature.properties.find("feature_type");
        if (feature_type_it == feature.properties.end()) {
            continue; // Skip features without type
        }
        
        if (feature_type_it->second == "zone_border") {
            // This is the main zone
            auto id_it = feature.properties.find("id");
            auto name_it = feature.properties.find("name");
            auto type_it = feature.properties.find("type");
            
            if (id_it != feature.properties.end()) {
                // Note: We can't set the zone ID directly in current implementation
                // The zone will get a new UUID
            }
            
            if (name_it != feature.properties.end()) {
                zone.setName(name_it->second);
            }
            
            if (type_it != feature.properties.end()) {
                zone.setType(type_it->second);
            }
            
            // Set border if it's a polygon
            if (std::holds_alternative<concord::Polygon>(feature.geometry)) {
                zone.setBorder(std::get<concord::Polygon>(feature.geometry));
            }
            
            // Set owner if present
            auto owner_it = feature.properties.find("owner_robot");
            if (owner_it != feature.properties.end()) {
                try {
                    zone.setOwnerRobot(uuidFromString(owner_it->second));
                } catch (const std::exception&) {
                    // Ignore invalid UUID
                }
            }
            
            // Restore zone properties (those with "prop_" prefix)
            for (const auto& [key, value] : feature.properties) {
                if (key.substr(0, 5) == "prop_") {
                    zone.setProperty(key.substr(5), value);
                }
            }
        }
        else if (feature_type_it->second == "subzone") {
            // This is a subzone
            auto name_it = feature.properties.find("name");
            auto type_it = feature.properties.find("type");
            
            if (name_it != feature.properties.end() && 
                type_it != feature.properties.end() &&
                std::holds_alternative<concord::Polygon>(feature.geometry)) {
                
                // Use the type string directly
                Subzone subzone(name_it->second, type_it->second, 
                               std::get<concord::Polygon>(feature.geometry));
                
                // Restore subzone properties
                for (const auto& [key, value] : feature.properties) {
                    if (key.substr(0, 5) == "prop_") {
                        subzone.setProperty(key.substr(5), value);
                    }
                }
                
                subzones.push_back(subzone);
            }
        }
    }
    
    // Add all subzones to the zone
    for (const auto& subzone : subzones) {
        zone.addSubzone(subzone);
    }
    
    return zone;
}

// Save zone to GeoJSON file (vector data)
void saveZoneToGeoJSON(const Zone& zone, const std::filesystem::path& path) {
    try {
        auto collection = zoneToGeoJSON(zone);
        geoson::WriteFeatureCollection(collection, path, geoson::CRS::ENU);
    } catch (const std::exception& e) {
        throw ZoneIOError("Failed to save zone to GeoJSON: " + std::string(e.what()));
    }
}

// Load zone from GeoJSON file
Zone loadZoneFromGeoJSON(const std::filesystem::path& path) {
    try {
        auto collection = geoson::ReadFeatureCollection(path);
        return geoJSONToZone(collection);
    } catch (const std::exception& e) {
        throw ZoneIOError("Failed to load zone from GeoJSON: " + std::string(e.what()));
    }
}

// Save zone layers to GeoTIFF file (raster data)
void saveLayersToGeoTIFF(const std::vector<ZoneLayer>& layers, const std::filesystem::path& path) {
    if (layers.empty()) {
        throw ZoneIOError("No layers to save");
    }
    
    try {
        geotiv::RasterCollection collection;
        collection.crs = geotiv::CRS::ENU;
        collection.datum = concord::Datum{};
        collection.heading = concord::Euler{};
        collection.resolution = layers[0].resolution; // Use first layer's resolution
        
        for (const auto& zone_layer : layers) {
            auto geotiv_layer = zoneLayerToGeotiv(zone_layer);
            collection.layers.push_back(geotiv_layer);
        }
        
        geotiv::WriteRasterCollection(collection, path);
    } catch (const std::exception& e) {
        throw ZoneIOError("Failed to save layers to GeoTIFF: " + std::string(e.what()));
    }
}

// Load zone layers from GeoTIFF file
std::vector<ZoneLayer> loadLayersFromGeoTIFF(const std::filesystem::path& path) {
    try {
        auto collection = geotiv::ReadRasterCollection(path);
        std::vector<ZoneLayer> layers;
        
        for (size_t i = 0; i < collection.layers.size(); ++i) {
            const auto& geotiv_layer = collection.layers[i];
            
            // Extract layer name and type from imageDescription
            std::string name = "layer_" + std::to_string(i);
            std::string type = "unknown";
            
            if (geotiv_layer.imageDescription.find("Layer: ") != std::string::npos) {
                size_t start = geotiv_layer.imageDescription.find("Layer: ") + 7;
                size_t end = geotiv_layer.imageDescription.find(",", start);
                if (end != std::string::npos) {
                    name = geotiv_layer.imageDescription.substr(start, end - start);
                }
            }
            
            if (geotiv_layer.imageDescription.find("Type: ") != std::string::npos) {
                size_t start = geotiv_layer.imageDescription.find("Type: ") + 6;
                size_t end = geotiv_layer.imageDescription.find(",", start);
                if (end != std::string::npos) {
                    type = geotiv_layer.imageDescription.substr(start, end - start);
                }
            }
            
            auto zone_layer = geotivToZoneLayer(geotiv_layer, name, type);
            layers.push_back(zone_layer);
        }
        
        return layers;
    } catch (const std::exception& e) {
        throw ZoneIOError("Failed to load layers from GeoTIFF: " + std::string(e.what()));
    }
}

// Save complete zone (GeoJSON + GeoTIFF)
void saveZone(const Zone& zone, const std::string& base_path) {
    std::string geojson_path = base_path + ".geojson";
    std::string geotiff_path = base_path + ".tiff";
    
    // Save vector data (border, subzones, metadata)
    saveZoneToGeoJSON(zone, geojson_path);
    
    // Save raster data (layers) if any exist
    if (zone.numLayers() > 0) {
        saveLayersToGeoTIFF(zone.getLayers(), geotiff_path);
    }
}

// Load complete zone (GeoJSON + GeoTIFF)
Zone loadZone(const std::string& base_path) {
    std::string geojson_path = base_path + ".geojson";
    std::string geotiff_path = base_path + ".tiff";
    
    // Load vector data
    Zone zone = loadZoneFromGeoJSON(geojson_path);
    
    // Load raster data if file exists
    if (std::filesystem::exists(geotiff_path)) {
        auto layers = loadLayersFromGeoTIFF(geotiff_path);
        for (const auto& layer : layers) {
            zone.addLayer(layer);
        }
    }
    
    return zone;
}

// Utility functions for file validation
bool isValidZoneFile(const std::string& base_path) {
    std::string geojson_path = base_path + ".geojson";
    return std::filesystem::exists(geojson_path);
}

std::vector<std::string> findZoneFiles(const std::filesystem::path& directory) {
    std::vector<std::string> zone_files;
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return zone_files;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".geojson") {
            std::string base_name = entry.path().stem().string();
            zone_files.push_back((directory / base_name).string());
        }
    }
    
    return zone_files;
}

} // namespace zone_io
} // namespace zoneout