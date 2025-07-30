# GeoTIFF QGIS Compatibility Analysis

## Overview

This document analyzes the geotiv library's GeoTIFF implementation and its compatibility with QGIS, based on examination of the source code and GeoTIFF standard requirements.

## GeoTIFF Implementation Analysis

### Core Implementation Files

- `/build/_deps/geotiv-src/include/geotiv/geotiv.hpp` - Main header
- `/build/_deps/geotiv-src/include/geotiv/raster.hpp` - Raster data structures
- `/build/_deps/geotiv-src/include/geotiv/writter.hpp` - Writing functionality
- `/build/_deps/geotiv-src/include/geotiv/parser.hpp` - Reading functionality
- `/build/_deps/geotiv-src/include/geotiv/types.hpp` - Data types and structures

### GeoTIFF Writing Functionality

The geotiv library implements GeoTIFF writing through:

**Writer Implementation (`writter.hpp`):**
- `toTiffBytes()` function creates complete TIFF byte streams
- `WriteRasterCollection()` saves to disk
- Multi-IFD (Image File Directory) support for multi-layer files
- Proper TIFF header construction with little-endian format
- Strip-based data organization

**Key Features:**
- 8-bit grayscale raster support
- Multi-layer support (each layer as separate IFD)
- Chunky pixel format (samples interleaved)
- Uncompressed TIFF format
- Proper byte ordering and alignment

### Coordinate System Handling

**Coordinate Reference Systems:**
- `geotiv::CRS::WGS` - WGS84 geographic coordinates
- `geotiv::CRS::ENU` - East-North-Up local coordinates

**Per-Layer Geospatial Metadata:**
- Independent datum, heading, and resolution per IFD
- Coordinate transformations handled by Concord library
- Automatic conversion between WGS84 and local ENU space

### TIFF Tags Implementation

**Standard TIFF Tags (per IFD):**
- 256: ImageWidth
- 257: ImageLength  
- 258: BitsPerSample (8-bit)
- 259: Compression (1 = uncompressed)
- 262: PhotometricInterpretation (1 = BlackIsZero)
- 270: ImageDescription (with geospatial metadata)
- 273: StripOffsets
- 277: SamplesPerPixel
- 278: RowsPerStrip
- 279: StripByteCounts
- 284: PlanarConfiguration (1 = chunky)

**GeoTIFF-Specific Tags:**
- 33550: ModelPixelScaleTag (X, Y, Z pixel scales)
- 34735: GeoKeyDirectoryTag (coordinate system keys)
- Custom tags (50000+) for application-specific metadata

## QGIS Compatibility Assessment

### ✅ Will Work (Basic Level)

- **TIFF Format**: Valid TIFF headers and structure
- **Raster Data**: 8-bit grayscale rasters load correctly
- **Basic Geometry**: Pixel dimensions and data organization are standard
- **File Structure**: Multi-layer support through multiple IFDs

### ⚠️ Potential Issues

#### 1. Coordinate System Recognition
- Uses custom ImageDescription format for CRS info
- Limited GeoTIFF key support (only basic GTModelTypeGeoKey)
- Missing comprehensive EPSG code support
- May default to "unknown projection" requiring manual CRS assignment

#### 2. GeoTIFF Compliance
- Minimal GeoKeyDirectory implementation
- Missing standard GeoTIFF keys for full compliance
- Custom metadata storage may not be recognized

#### 3. Projection Information
- Simplified CRS enum (WGS vs ENU) instead of full projection parameters
- No support for complex projections (UTM, State Plane, etc.)
- Missing datum transformation parameters

#### 4. Format Limitations
- Only strip-based organization (no tiles)
- No compression support
- Fixed uncompressed format may cause large files
- Limited to 8-bit grayscale (no RGB or palette support)

### Expected QGIS Behavior

```
✅ QGIS will open the files (valid TIFF format)
⚠️ Coordinate system may show as "Unknown CRS" 
⚠️ May need manual CRS assignment in QGIS
✅ Raster data will display correctly (grayscale)
⚠️ Geospatial positioning may be approximate
⚠️ Limited styling options (grayscale only)
```

## Testing Recommendations

### Quick Compatibility Test

1. **Save a zone raster**: 
   ```cpp
   zone.toFiles("/tmp/test.geojson", "/tmp/test.tiff");
   ```

2. **Open in QGIS**: 
   - Load the `/tmp/test.tiff` file
   - Observe any warnings or errors

3. **Check CRS**: 
   - See if projection is automatically recognized
   - Check if it shows as "Unknown CRS"

4. **Verify positioning**: 
   - Check if raster aligns with expected coordinates
   - Compare with vector data if available

### Validation Tools

- **GDAL Info**: `gdalinfo /tmp/test.tiff` to check GeoTIFF metadata
- **QGIS CRS Dialog**: Check coordinate system recognition
- **Reference Comparison**: Compare with known-good GeoTIFF files

## Recommendations for Improvement

### 1. Enhanced GeoTIFF Key Support
- Add comprehensive GeoKeyDirectory entries
- Include EPSG codes and standard projection parameters
- Implement GeoAsciiParamsTag and GeoDoubleParamsTag

### 2. Improved Coordinate System Metadata
- Use standard EPSG codes instead of custom CRS enum
- Add proper datum transformation parameters
- Include complete projection information

### 3. Expanded Format Support
- Add RGB and palette color models
- Implement tile-based organization option
- Add compression support (LZW, JPEG)

### 4. Validation and Testing
- Test with QGIS to verify actual compatibility
- Use GDAL tools to validate GeoTIFF compliance
- Compare with reference GeoTIFF files

## Workarounds for Current Issues

### If CRS Recognition Fails
1. **Manual CRS Assignment**: Set to WGS84 (EPSG:4326) in QGIS
2. **Coordinate Adjustment**: Use QGIS georeferencing tools
3. **Layer Properties**: Manually configure coordinate system

### If Positioning is Incorrect
1. **Georeferencing**: Use QGIS georeferencing plugin
2. **Ground Control Points**: Add reference points if available
3. **Coordinate Transformation**: Apply manual transformations

### If Performance is Poor
1. **File Compression**: Convert to compressed format using GDAL
2. **Pyramids**: Build overviews for large rasters
3. **Tiling**: Convert to tiled format for better performance

## Conclusion

The current geotiv implementation provides a solid foundation for GeoTIFF support and should work with QGIS for basic visualization. However, it may require manual CRS assignment and has limitations in format support. For production use, consider implementing the recommended improvements to enhance compatibility and compliance with GeoTIFF standards.

**Bottom Line**: It should work for basic visualization, but may need manual CRS setup in QGIS for proper geospatial positioning.