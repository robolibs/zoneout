#pragma once

#include <cstddef>

namespace zoneout {

    // Default values for path clearance and occlusion checking
    constexpr double DEFAULT_PATH_CLEAR_THRESHOLD = 50.0;
    constexpr double DEFAULT_ROBOT_HEIGHT = 2.0;
    constexpr std::size_t DEFAULT_PATH_SAMPLES = 20;

    // Default values for zone resolution
    constexpr double DEFAULT_RESOLUTION = 1.0;

    // Default occlusion layer configuration
    constexpr std::size_t DEFAULT_HEIGHT_LAYERS = 10;
    constexpr double DEFAULT_LAYER_HEIGHT = 1.0;

    // Default subtype
    inline constexpr const char *DEFAULT_SUBTYPE = "default";

    // Default occlusion layer metadata
    inline constexpr const char *DEFAULT_OCCLUSION_NAME = "occlusion_map";
    inline constexpr const char *DEFAULT_OCCLUSION_TYPE = "occlusion";
    inline constexpr const char *DEFAULT_OCCLUSION_SUBTYPE = "robot_navigation";

} // namespace zoneout
