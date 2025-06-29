#pragma once

// Zoneout library - Agricultural zone management for robotics
// Header-only library for managing robotic operation zones and spaces

#include "zoneout/zoneout/utils/time.hpp"
#include "zoneout/zoneout/utils/uuid.hpp"
#include "zoneout/zoneout/zone.hpp"
#include "visualize.hpp"

namespace zoneout {
    // Library version
    constexpr int VERSION_MAJOR = 0;
    constexpr int VERSION_MINOR = 1;
    constexpr int VERSION_PATCH = 0;

    inline std::string getVersion() {
        return std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." +
               std::to_string(VERSION_PATCH);
    }
} // namespace zoneout
