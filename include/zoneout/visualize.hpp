#pragma once

#ifdef HAS_RERUN

#include "zoneout/zone.hpp"
#include <rerun/recording_stream.hpp>
#include <rerun/result.hpp>
#include <rerun/archetypes/line_strips3d.hpp>
#include <rerun/archetypes/geo_line_strings.hpp>
#include <rerun/components/color.hpp>
#include <rerun/components/radius.hpp>
#include <rerun/components/position3d.hpp>
#include <rerun/components/geo_line_string.hpp>
#include <rerun/datatypes/dvec2d.hpp>
#include <vector>
#include <array>
#include <cmath>

namespace zoneout {
namespace visualize {

// Simple function to visualize a single zone - ENU and WGS coordinates
inline void visualize_zone(const Zone& zone, std::shared_ptr<rerun::RecordingStream> rec, 
                          const concord::Datum& datum, const std::string& zone_name, 
                          size_t color_index = 0) {
    if (!zone.has_boundary()) return;
    
    const auto& boundary = zone.get_boundary();
    const auto& points_data = boundary.getPoints();
    if (points_data.empty()) return;
    
    // Color palette
    static const std::vector<rerun::components::Color> palette = {
        rerun::components::Color{255, 100, 100},  // Red
        rerun::components::Color{100, 255, 100},  // Green  
        rerun::components::Color{100, 100, 255},  // Blue
        rerun::components::Color{255, 255, 100},  // Yellow
        rerun::components::Color{100, 255, 255},  // Cyan
        rerun::components::Color{255, 100, 255},  // Magenta
        rerun::components::Color{255, 150, 100},  // Orange
        rerun::components::Color{150, 100, 255},  // Purple
        rerun::components::Color{100, 255, 150},  // Light Green
        rerun::components::Color{255, 100, 150}   // Pink
    };
    
    auto color = palette[color_index % palette.size()];
    
    // ENU coordinates visualization (local 3D space)
    std::vector<std::array<float, 3>> enu_points;
    for (const auto& point : points_data) {
        enu_points.push_back({float(point.x), float(point.y), float(point.z)});
    }
    // Close the polygon
    if (!enu_points.empty()) {
        enu_points.push_back(enu_points.front());
    }
    
    rec->log("/" + zone_name + "/enu", 
             rerun::archetypes::LineStrips3D(rerun::components::LineStrip3D(enu_points))
                 .with_colors({color})
                 .with_radii({0.5f}));
    
    // WGS coordinates visualization (GPS on map)
    std::vector<rerun::datatypes::DVec2D> wgs_coords;
    for (const auto& point : points_data) {
        // Convert ENU to WGS (simplified conversion)
        double lat = datum.lat + (point.y / 111000.0); // ~111km per degree latitude
        double lon = datum.lon + (point.x / (111000.0 * std::cos(datum.lat * M_PI / 180.0))); // longitude adjusted for latitude
        wgs_coords.emplace_back(lat, lon);
    }
    // Close polygon
    if (!wgs_coords.empty()) {
        wgs_coords.push_back(wgs_coords.front());
        
        auto geo_line_string = rerun::components::GeoLineString::from_lat_lon(wgs_coords);
        rec->log("/" + zone_name + "/wgs",
                 rerun::archetypes::GeoLineStrings(geo_line_string)
                     .with_colors({color})
                     .with_radii({2.0f}));
    }
}


} // namespace visualize
} // namespace zoneout

#endif // HAS_RERUN