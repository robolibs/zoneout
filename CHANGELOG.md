# Changelog

## [0.3.3] - 2025-10-31

### <!-- 0 -->⛰️  Features

- Introduce spatially-referenced data layer

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Update dependency versions for concord and geotiv
- Update `geotiv` to version 2.0.2

## [0.3.2] - 2025-08-08

### <!-- 6 -->🧪 Testing

- Enhance grid and polygon robustness for varied real-world data

## [0.3.1] - 2025-08-08

### <!-- 0 -->⛰️  Features

- Add Zone object (de)serialization

## [0.2.6] - 2025-08-08

### <!-- 1 -->🐛 Bug Fixes

- Reverse Y-axis for GIS/image coordinates

## [0.2.5] - 2025-08-04

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Clean up build system dependencies

## [0.2.4] - 2025-08-04

### <!-- 0 -->⛰️  Features

- Implement core ISOXML generation and plot persistence

### <!-- 2 -->🚜 Refactor

- Refactor field creation and improve zone visualization
- Adopt Rerun for enhanced visualization capabilities
- Refactor plot loading and zone management

### <!-- 3 -->📚 Documentation

- Relocate documentation examples and remove unused tests

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Update GEOJSON and CRS handling

## [0.2.1] - 2025-07-30

### <!-- 0 -->⛰️  Features

- Update geotiff processing and documentation

### <!-- 2 -->🚜 Refactor

- Disable function `record_api_data` call

### <!-- 3 -->📚 Documentation

- Add plotting classes and examples

## [0.2.0] - 2025-07-09

### <!-- 0 -->⛰️  Features

- Refactor Plot to handle tar file saving and loading
- Add plot management system and I/O capabilities
- Add `microtar` archive processing library

## [0.1.0] - 2025-07-08

### <!-- 0 -->⛰️  Features

- Refactor polygon creation and initialization logic
- Refactor grid generation using AABB and add debug info
- Refactor and enhance geospatial processing with improved polygon tools

### <!-- 2 -->🚜 Refactor

- Refactor zone management and integrate polygon features

### <!-- 4 -->⚡ Performance

- Use robust point-in-polygon instead of `indices_within`

### <!-- 7 -->⚙️ Miscellaneous Tasks

- Remove pigment as an external dependency

## [0.0.4] - 2025-07-03

### <!-- 0 -->⛰️  Features

- Enhance zone generation and raster data handling

## [0.0.3] - 2025-06-29

### <!-- 2 -->🚜 Refactor

- Refactor Zone to manage raster data through an initial grid
- Refactor: improve boundary handling in structured elements

## [0.0.2] - 2025-06-29

### <!-- 0 -->⛰️  Features

- Unify spatial data handling and grid generation
- Refactor element handling and introduce structured types
- Introduce poly and grid geometry management classes
- Refactor zone property management for global consistency
- Improve file I/O for `Zone` and documentation of `Farm`
- Revamp zone and farm management with spatial data types
- Introduce `Zone` class for agricultural area management
- Introduce UUID and time utilities
- Init

### <!-- 2 -->🚜 Refactor

- Harden `Zone` class to use composition over inheritance
- Refactor `Zone` for direct construction and external raster use
- Refactor API for improved consistency and generalization
- Refactor: Introduce Zone class for better organization

### <!-- 3 -->📚 Documentation

- Refactor documentation into a comprehensive user guide
- Update project description

