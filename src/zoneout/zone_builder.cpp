#include "zoneout/zoneout/zone_builder.hpp"

namespace zoneout {

    // ========== Required Configuration Methods ==========

    ZoneBuilder &ZoneBuilder::with_name(const std::string &name) {
        name_ = name;
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_type(const std::string &type) {
        type_ = type;
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_boundary(const concord::Polygon &boundary) {
        boundary_ = boundary;
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_datum(const concord::Datum &datum) {
        datum_ = datum;
        return *this;
    }

    // ========== Optional Configuration Methods ==========

    ZoneBuilder &ZoneBuilder::with_resolution(double resolution) {
        resolution_ = resolution;
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_initial_grid(const concord::Grid<uint8_t> &grid) {
        initial_grid_ = grid;
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_property(const std::string &key, const std::string &value) {
        properties_[key] = value;
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_properties(const std::unordered_map<std::string, std::string> &properties) {
        for (const auto &[key, value] : properties) {
            properties_[key] = value;
        }
        return *this;
    }

    // ========== Feature Configuration Methods ==========

    ZoneBuilder &ZoneBuilder::with_raster_layer(const concord::Grid<uint8_t> &grid, const std::string &name,
                                                const std::string &type,
                                                const std::unordered_map<std::string, std::string> &properties,
                                                bool poly_cut, int layer_index) {
        RasterLayerConfig config;
        config.grid = grid;
        config.name = name;
        config.type = type;
        config.properties = properties;
        config.poly_cut = poly_cut;
        config.layer_index = layer_index;
        raster_layers_.push_back(config);
        return *this;
    }

    ZoneBuilder &ZoneBuilder::with_polygon_feature(const concord::Polygon &geometry, const std::string &name,
                                                   const std::string &type, const std::string &subtype,
                                                   const std::unordered_map<std::string, std::string> &properties) {
        PolygonFeatureConfig config;
        config.geometry = geometry;
        config.name = name;
        config.type = type;
        config.subtype = subtype;
        config.properties = properties;
        polygon_features_.push_back(config);
        return *this;
    }

    // ========== Validation ==========

    bool ZoneBuilder::is_valid() const { return validation_error().empty(); }

    std::string ZoneBuilder::validation_error() const {
        if (!name_.has_value() || name_->empty()) {
            return "Zone name is required and cannot be empty";
        }

        if (!type_.has_value() || type_->empty()) {
            return "Zone type is required and cannot be empty";
        }

        if (!boundary_.has_value()) {
            return "Zone boundary is required";
        }

        if (boundary_->getPoints().size() < 3) {
            return "Boundary polygon must have at least 3 points";
        }

        if (!datum_.has_value()) {
            return "Zone datum is required";
        }

        if (resolution_ <= 0.0) {
            return "Resolution must be positive (got " + std::to_string(resolution_) + ")";
        }

        return "";
    }

    // ========== Building ==========

    Zone ZoneBuilder::build() const {
        // Validate before building
        std::string error = validation_error();
        if (!error.empty()) {
            throw std::invalid_argument("ZoneBuilder validation failed: " + error);
        }

        // Create zone with either initial grid or auto-generated grid
        Zone zone = initial_grid_.has_value()
                        ? Zone(name_.value(), type_.value(), boundary_.value(), initial_grid_.value(), datum_.value())
                        : Zone(name_.value(), type_.value(), boundary_.value(), datum_.value(), resolution_);

        // Add properties
        for (const auto &[key, value] : properties_) {
            zone.set_property(key, value);
        }

        // Add raster layers
        for (const auto &layer : raster_layers_) {
            zone.add_raster_layer(layer.grid, layer.name, layer.type, layer.properties, layer.poly_cut,
                                  layer.layer_index);
        }

        // Add polygon features
        for (const auto &feature : polygon_features_) {
            zone.add_polygon_feature(feature.geometry, feature.name, feature.type, feature.subtype, feature.properties);
        }

        return zone;
    }

    // ========== Reset ==========

    void ZoneBuilder::reset() {
        name_.reset();
        type_.reset();
        boundary_.reset();
        datum_.reset();
        resolution_ = 1.0;
        initial_grid_.reset();
        properties_.clear();
        raster_layers_.clear();
        polygon_features_.clear();
    }

} // namespace zoneout
