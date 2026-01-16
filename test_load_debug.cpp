#include "zoneout/zoneout.hpp"
#include <iostream>

int main() {
    try {
        std::cout << "Attempting to load plot from /home/bresilla/farm_plot_2..." << std::endl;

        // Try to load with detailed error reporting
        auto farm = zoneout::Plot::load("/home/bresilla/farm_plot_2", "Pea Farm", "agricultural");

        std::cout << "Plot loaded successfully!" << std::endl;
        std::cout << "Number of zones: " << farm.get_zone_count() << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error loading plot: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
