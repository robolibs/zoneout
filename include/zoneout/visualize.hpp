#pragma once

#ifdef HAS_RERUN

#include <datapod/datapod.hpp>

#include "zoneout/zone.hpp"
#include <array>
#include <rerun/archetypes/geo_line_strings.hpp>
#include <rerun/archetypes/line_strips3d.hpp>
#include <rerun/components/color.hpp>
#include <rerun/components/geo_line_string.hpp>
#include <rerun/components/lat_lon.hpp>
#include <rerun/recording_stream.hpp>
#include <rerun/result.hpp>
#include <vector>

#include <concord/frame/convert.hpp>

namespace zoneout {
    namespace visualize {

        /**
         * @brief Convert ENU point to WGS84 LatLon for geo visualization
         */
        inline rerun::components::LatLon enu_to_latlon(const datapod::Point &enu_pt, const datapod::Geo &datum) {
            // Create ENU with origin
            concord::frame::ENU enu{enu_pt.x, enu_pt.y, enu_pt.z, datum};
            // Convert to WGS
            auto wgs = concord::frame::to_wgs(enu);
            return rerun::components::LatLon{float(wgs.latitude), float(wgs.longitude)};
        }

        /**
         * @brief Visualize a single zone - ENU and WGS coordinates
         */
        inline void show_zone(const Zone &zone, std::shared_ptr<rerun::RecordingStream> rec, const datapod::Geo &datum,
                              const std::string &zone_name, size_t color_index = 0) {
            if (!zone.poly().has_field_boundary()) {
                std::cerr << "Zone " << zone_name << " has no field boundary" << std::endl;
                return;
            }

            const auto &boundary = zone.poly().field_boundary();
            const auto &vertices = boundary.vertices;
            if (vertices.empty()) {
                std::cerr << "Zone " << zone_name << " has no points" << std::endl;
                return;
            }

            // Color palette
            static const std::vector<rerun::components::Color> palette = {
                rerun::components::Color{255, 100, 100}, // Red
                rerun::components::Color{100, 255, 100}, // Green
                rerun::components::Color{100, 100, 255}, // Blue
                rerun::components::Color{255, 255, 100}, // Yellow
                rerun::components::Color{100, 255, 255}, // Cyan
                rerun::components::Color{255, 100, 255}, // Magenta
                rerun::components::Color{255, 150, 100}, // Orange
                rerun::components::Color{150, 100, 255}, // Purple
                rerun::components::Color{100, 255, 150}, // Light Green
                rerun::components::Color{255, 100, 150}  // Pink
            };

            auto color = palette[color_index % palette.size()];

            // ENU coordinates visualization (local 3D space)
            std::vector<std::array<float, 3>> enu_points;
            std::vector<rerun::components::LatLon> wgs_coords;
            for (const auto &point : vertices) {
                enu_points.push_back({float(point.x), float(point.y), 0.0f});
                wgs_coords.push_back(enu_to_latlon(point, datum));
            }

            // Close the polygon if not already closed
            if (!enu_points.empty() &&
                (enu_points.front()[0] != enu_points.back()[0] || enu_points.front()[1] != enu_points.back()[1])) {
                enu_points.push_back(enu_points.front());
                wgs_coords.push_back(wgs_coords.front());
            }

            std::cout << "Visualizing zone: " << zone_name << " with " << enu_points.size() << " points" << std::endl;

            // Log ENU coordinates (3D view)
            rec->log_static("/" + zone_name + "/enu",
                            rerun::archetypes::LineStrips3D(rerun::components::LineStrip3D(enu_points))
                                .with_colors({{color}})
                                .with_radii({{0.5f}}));

            // Log WGS84 coordinates (Map view)
            auto geo_line_string = rerun::components::GeoLineString::from_lat_lon(wgs_coords);
            rec->log_static(
                "/" + zone_name + "/wgs",
                rerun::archetypes::GeoLineStrings(geo_line_string).with_colors({{color}}).with_radii({{2.0f}}));
        }

        /**
         * @brief Backward compatible version without datum (no geo visualization)
         */
        inline void show_zone(const Zone &zone, std::shared_ptr<rerun::RecordingStream> rec,
                              const std::string &zone_name, size_t color_index = 0) {
            if (!zone.poly().has_field_boundary()) {
                std::cerr << "Zone " << zone_name << " has no field boundary" << std::endl;
                return;
            }

            const auto &boundary = zone.poly().field_boundary();
            const auto &vertices = boundary.vertices;
            if (vertices.empty()) {
                std::cerr << "Zone " << zone_name << " has no points" << std::endl;
                return;
            }

            // Color palette
            static const std::vector<rerun::components::Color> palette = {
                rerun::components::Color{255, 100, 100}, // Red
                rerun::components::Color{100, 255, 100}, // Green
                rerun::components::Color{100, 100, 255}, // Blue
                rerun::components::Color{255, 255, 100}, // Yellow
                rerun::components::Color{100, 255, 255}, // Cyan
                rerun::components::Color{255, 100, 255}, // Magenta
                rerun::components::Color{255, 150, 100}, // Orange
                rerun::components::Color{150, 100, 255}, // Purple
                rerun::components::Color{100, 255, 150}, // Light Green
                rerun::components::Color{255, 100, 150}  // Pink
            };

            auto color = palette[color_index % palette.size()];

            // ENU coordinates visualization (local 3D space)
            std::vector<std::array<float, 3>> enu_points;
            for (const auto &point : vertices) {
                enu_points.push_back({float(point.x), float(point.y), 0.0f});
            }

            // Close the polygon if not already closed
            if (!enu_points.empty() &&
                (enu_points.front()[0] != enu_points.back()[0] || enu_points.front()[1] != enu_points.back()[1])) {
                enu_points.push_back(enu_points.front());
            }

            std::cout << "Visualizing zone: " << zone_name << " with " << enu_points.size() << " points" << std::endl;

            // Log ENU coordinates (3D view)
            rec->log_static("/" + zone_name + "/enu",
                            rerun::archetypes::LineStrips3D(rerun::components::LineStrip3D(enu_points))
                                .with_colors({{color}})
                                .with_radii({{0.5f}}));
        }

        /**
         * @brief Visualize polygon elements within a zone
         */
        inline void show_polygon_elements(const Zone &zone, std::shared_ptr<rerun::RecordingStream> rec,
                                          const datapod::Geo &datum, const std::string &zone_name,
                                          float height = 0.1f) {
            const auto &elements = zone.poly().polygon_elements();

            for (size_t i = 0; i < elements.size(); ++i) {
                const auto &element = elements[i];
                const auto &vertices = element.geometry.vertices;

                std::vector<std::array<float, 3>> enu_points;
                std::vector<rerun::components::LatLon> wgs_coords;
                for (const auto &point : vertices) {
                    enu_points.push_back({float(point.x), float(point.y), height});
                    wgs_coords.push_back(enu_to_latlon(point, datum));
                }

                // Close polygon
                if (!enu_points.empty() &&
                    (enu_points.front()[0] != enu_points.back()[0] || enu_points.front()[1] != enu_points.back()[1])) {
                    enu_points.push_back(enu_points.front());
                    wgs_coords.push_back(wgs_coords.front());
                }

                std::string entity_path = "/" + zone_name + "/elements/" + element.type + std::to_string(i);

                // Log ENU coordinates (3D view)
                rec->log_static(entity_path, rerun::archetypes::LineStrips3D(rerun::components::LineStrip3D(enu_points))
                                                 .with_colors({{rerun::components::Color(200, 200, 100)}})
                                                 .with_radii({{0.3f}}));

                // Log WGS84 coordinates (Map view)
                auto geo_line_string = rerun::components::GeoLineString::from_lat_lon(wgs_coords);
                rec->log_static(entity_path, rerun::archetypes::GeoLineStrings(geo_line_string)
                                                 .with_colors({{rerun::components::Color(200, 200, 100)}})
                                                 .with_radii({{1.5f}}));
            }
        }

        /**
         * @brief Backward compatible version without datum (no geo visualization)
         */
        inline void show_polygon_elements(const Zone &zone, std::shared_ptr<rerun::RecordingStream> rec,
                                          const std::string &zone_name, float height = 0.1f) {
            const auto &elements = zone.poly().polygon_elements();

            for (size_t i = 0; i < elements.size(); ++i) {
                const auto &element = elements[i];
                const auto &vertices = element.geometry.vertices;

                std::vector<std::array<float, 3>> enu_points;
                for (const auto &point : vertices) {
                    enu_points.push_back({float(point.x), float(point.y), height});
                }

                // Close polygon
                if (!enu_points.empty() &&
                    (enu_points.front()[0] != enu_points.back()[0] || enu_points.front()[1] != enu_points.back()[1])) {
                    enu_points.push_back(enu_points.front());
                }

                std::string entity_path = "/" + zone_name + "/elements/" + element.type + std::to_string(i);

                // Log ENU coordinates (3D view)
                rec->log_static(entity_path, rerun::archetypes::LineStrips3D(rerun::components::LineStrip3D(enu_points))
                                                 .with_colors({{rerun::components::Color(200, 200, 100)}})
                                                 .with_radii({{0.3f}}));
            }
        }

    } // namespace visualize
} // namespace zoneout

#endif // HAS_RERUN
