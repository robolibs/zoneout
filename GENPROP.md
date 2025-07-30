# Plot Properties Persistence Implementation Guide

## Problem
Currently, Plot properties are not saved or loaded during file I/O operations. When you save a Plot and load it back, all custom properties (set via `setProperty()`) are lost.

## Current State
- Plot has `properties_` map and `setProperty()`/`getProperty()` methods
- Zone already saves its own properties with "prop_" prefix to GeoJSON/GeoTIFF
- Zone only loads back properties with "prop_" prefix (ignores others)
- Both GeoJSON and GeoTIFF support global metadata via `setGlobalProperty()`/`getGlobalProperty()`

## Solution: Embed Plot Properties in Zone Files

Use Zone's existing global property system to store Plot metadata with a different prefix ("plot_") so Zone ignores them but Plot can retrieve them.

### Implementation Steps

#### 1. Modify Plot::save() method (line ~89 in plot.hpp)

**Before saving each zone**, inject Plot properties:

```cpp
void save(const std::filesystem::path &directory) const {
    std::filesystem::create_directories(directory);

    for (size_t i = 0; i < zones_.size(); ++i) {
        auto zone_dir = directory / ("zone_" + std::to_string(i));
        std::filesystem::create_directories(zone_dir);
        auto vector_path = zone_dir / "vector.geojson";
        auto raster_path = zone_dir / "raster.tiff";
        
        // Create a copy of the zone to avoid modifying the original
        auto zone_copy = zones_[i];
        
        // Inject Plot metadata into the zone copy
        zone_copy.setGlobalProperty("plot_id", id_.toString().c_str());
        zone_copy.setGlobalProperty("plot_name", name_.c_str());
        zone_copy.setGlobalProperty("plot_type", type_.c_str());
        
        // Inject Plot custom properties with "plot_prop_" prefix
        for (const auto &[key, value] : properties_) {
            std::string plot_prop_key = "plot_prop_" + key;
            zone_copy.setGlobalProperty(plot_prop_key.c_str(), value);
        }
        
        // Save the zone with embedded Plot metadata
        zone_copy.toFiles(vector_path, raster_path);
    }
}
```

#### 2. Modify Plot::load() method (line ~218 in plot.hpp)

**After loading zones**, extract Plot properties from the first zone:

```cpp
static Plot load(const std::filesystem::path &directory, const std::string &name, const std::string &type,
                 const concord::Datum &datum = concord::Datum{0.001, 0.001, 1.0}) {
    Plot plot(name, type, datum);
    concord::Datum plot_datum;
    bool plot_metadata_loaded = false;

    if (std::filesystem::exists(directory)) {
        for (const auto &entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_directory() && entry.path().filename().string().starts_with("zone_")) {
                try {
                    auto vector_path = entry.path() / "vector.geojson";
                    auto raster_path = entry.path() / "raster.tiff";
                    auto zone = Zone::fromFiles(vector_path, raster_path);
                    plot_datum = zone.getDatum();
                    
                    // Extract Plot metadata from the first zone only
                    if (!plot_metadata_loaded) {
                        // Restore Plot UUID if available
                        std::string plot_id = zone.getGlobalProperty("plot_id");
                        if (!plot_id.empty()) {
                            plot.id_ = UUID::fromString(plot_id);
                        }
                        
                        // Restore Plot name and type if available (override parameters)
                        std::string plot_name = zone.getGlobalProperty("plot_name");
                        if (!plot_name.empty()) {
                            plot.name_ = plot_name;
                        }
                        
                        std::string plot_type = zone.getGlobalProperty("plot_type");
                        if (!plot_type.empty()) {
                            plot.type_ = plot_type;
                        }
                        
                        // Restore Plot custom properties
                        auto field_props = zone.getVectorData().getFieldProperties();
                        for (const auto &[key, value] : field_props) {
                            if (key.substr(0, 10) == "plot_prop_") {
                                plot.properties_[key.substr(10)] = value;
                            }
                        }
                        
                        plot_metadata_loaded = true;
                    }
                    
                    plot.addZone(zone);
                } catch (const std::exception &) {
                    // Skip invalid zone directories
                }
            }
        }
    }
    plot.datum_ = plot_datum;
    return plot;
}
```

#### 3. Update Plot::save_tar() method (line ~101 in plot.hpp)

The tar save method calls `save()` internally, so it will automatically get the Plot property injection:

```cpp
// In save_tar(), after line 110:
// Create temporary directory for zone files
auto temp_dir = std::filesystem::temp_directory_path() / ("plot_" + id_.toString());
save(temp_dir);  // This will now inject Plot properties into zones
```

No additional changes needed since `save_tar()` uses `save()` internally.

#### 4. Update Plot::load_tar() method (line ~159 in plot.hpp)

The tar load method calls `load()` internally, so it will automatically extract Plot properties:

```cpp
// In load_tar(), after line 210:
Plot plot = load(temp_dir, name, type, datum);  // This will now extract Plot properties
```

No additional changes needed since `load_tar()` uses `load()` internally.

### Property Prefix Scheme

| Prefix | Used By | Purpose | Example |
|--------|---------|---------|---------|
| `prop_` | Zone | Zone-specific properties | `prop_crop_type` |
| `plot_prop_` | Plot | Plot custom properties | `plot_prop_farm_type` |
| `plot_id` | Plot | Plot UUID | `plot_id` |
| `plot_name` | Plot | Plot name | `plot_name` |
| `plot_type` | Plot | Plot type | `plot_type` |

### Benefits

1. **No breaking changes** - existing Zone behavior unchanged
2. **No additional files** - uses existing GeoJSON/GeoTIFF metadata
3. **Works with tar files** - automatically included in tar save/load
4. **Zone ignores Plot data** - Zone only processes "prop_" prefix
5. **Preserves Plot UUID** - maintains identity across save/load cycles
6. **Backward compatible** - old files without Plot metadata still load

### Testing Strategy

1. **Create test case** in `test_file_io_roundtrip.cpp`:
   ```cpp
   TEST_CASE("Plot properties persistence") {
       // Create Plot with properties
       Plot original_plot("Test Farm", "agricultural", datum);
       original_plot.setProperty("owner", "John Doe");
       original_plot.setProperty("farm_type", "organic");
       
       // Add zones and save
       // ... add zones ...
       original_plot.save("/tmp/test_plot");
       
       // Load and verify properties
       auto loaded_plot = Plot::load("/tmp/test_plot", "name", "type", datum);
       CHECK(loaded_plot.getProperty("owner") == "John Doe");
       CHECK(loaded_plot.getProperty("farm_type") == "organic");
       CHECK(loaded_plot.getId() == original_plot.getId());
   }
   ```

2. **Test tar roundtrip** with properties
3. **Test backward compatibility** with existing files

### Edge Cases to Handle

1. **Empty Plot** - no zones to store metadata in
2. **Multiple zones** - only store Plot metadata in first zone
3. **Corrupted metadata** - graceful fallback to provided parameters
4. **Property name conflicts** - ensure "plot_prop_" prefix is unique

### Future Enhancements

1. **Compression** - if Plot properties become large
2. **Versioning** - add `plot_version` property for future compatibility
3. **Validation** - verify Plot metadata consistency across zones