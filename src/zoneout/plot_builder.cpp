#include "zoneout/zoneout/plot_builder.hpp"

namespace zoneout {

    // ========== Required Configuration Methods ==========

    PlotBuilder &PlotBuilder::with_name(const std::string &name) {
        name_ = name;
        return *this;
    }

    PlotBuilder &PlotBuilder::with_type(const std::string &type) {
        type_ = type;
        return *this;
    }

    PlotBuilder &PlotBuilder::with_datum(const concord::Datum &datum) {
        datum_ = datum;
        return *this;
    }

    // ========== Optional Configuration Methods ==========

    PlotBuilder &PlotBuilder::with_property(const std::string &key, const std::string &value) {
        properties_[key] = value;
        return *this;
    }

    PlotBuilder &PlotBuilder::with_properties(const std::unordered_map<std::string, std::string> &properties) {
        for (const auto &[key, value] : properties) {
            properties_[key] = value;
        }
        return *this;
    }

    // ========== Zone Management Methods ==========

    PlotBuilder &PlotBuilder::add_zone(const Zone &zone) {
        zones_.push_back(zone);
        return *this;
    }

    PlotBuilder &PlotBuilder::add_zone(Zone &&zone) {
        zones_.push_back(std::move(zone));
        return *this;
    }

    PlotBuilder &PlotBuilder::add_zone(std::function<void(ZoneBuilder &)> configurator) {
        zone_configs_.push_back(configurator);
        return *this;
    }

    PlotBuilder &PlotBuilder::add_zones(const std::vector<Zone> &zones) {
        for (const auto &zone : zones) {
            zones_.push_back(zone);
        }
        return *this;
    }

    // ========== Validation ==========

    bool PlotBuilder::is_valid() const { return validation_error().empty(); }

    std::string PlotBuilder::validation_error() const {
        if (!name_.has_value() || name_->empty()) {
            return "Plot name is required and cannot be empty";
        }

        if (!type_.has_value() || type_->empty()) {
            return "Plot type is required and cannot be empty";
        }

        if (!datum_.has_value()) {
            return "Plot datum is required";
        }

        return "";
    }

    // ========== Building ==========

    Plot PlotBuilder::build() const {
        // Validate before building
        std::string error = validation_error();
        if (!error.empty()) {
            throw std::invalid_argument("PlotBuilder validation failed: " + error);
        }

        // Create plot
        Plot plot(name_.value(), type_.value(), datum_.value());

        // Add properties
        for (const auto &[key, value] : properties_) {
            plot.set_property(key, value);
        }

        // Add pre-built zones
        for (const auto &zone : zones_) {
            plot.add_zone(zone);
        }

        // Build and add deferred zones from configurators
        for (const auto &config : zone_configs_) {
            ZoneBuilder builder;

            // Use the plot's datum as default if zone doesn't specify one
            builder.with_datum(datum_.value());

            // Apply configuration
            config(builder);

            // Validate and build zone
            if (!builder.is_valid()) {
                throw std::invalid_argument("Zone configuration invalid in PlotBuilder: " + builder.validation_error());
            }

            plot.add_zone(builder.build());
        }

        return plot;
    }

    // ========== Reset ==========

    void PlotBuilder::reset() {
        name_.reset();
        type_.reset();
        datum_.reset();
        properties_.clear();
        zones_.clear();
        zone_configs_.clear();
    }

    // ========== Utility Methods ==========

    size_t PlotBuilder::zone_count() const { return zones_.size() + zone_configs_.size(); }

} // namespace zoneout
