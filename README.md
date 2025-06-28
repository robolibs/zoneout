
<img align="right" width="26%" src="./misc/logo.png">

Zoneout
===

A Header-Only C++ Library for Advanced Workspace Zone Engineering with Raster & Vector Map Integration

Zoneout provides comprehensive zone management for agricultural robotics, enabling distributed coordination, spatial reasoning, and efficient workspace partitioning for autonomous farming operations.

---

## How Zoning Works

### Core Concepts

Zoneout implements a hierarchical spatial management system designed for multi-robot agricultural coordination with **flexible string-based typing** that supports unlimited zone and subzone categories:

```
Farm
├── Zone (Field)
│   ├── Border (Polygon)
│   ├── Layers (Raster Data)
│   │   ├── Elevation
│   │   ├── Soil Properties
│   │   └── Crop Health
│   └── Subzones
│       ├── Crop Rows
│       ├── Irrigation Areas
│       └── Access Paths
├── Zone (Barn)
│   ├── Border (Polygon)
│   └── Subzones
│       ├── Milking Stations
│       ├── Feeding Areas
│       └── Storage
└── Zone (Road)
    ├── Border (Polygon)
    └── Access Control
```

### Zone Architecture Flow

```
       ┌─────────────────┐
       │  Zone Creation  │
       └───────┬─────────┘
               │
               ▼
       ┌─────────────────┐
       │Border Definition│
       └───────┬─────────┘
               │
               ▼
       ┌─────────────────┐
       │ Layer Addition  │
       └───────┬─────────┘
               │
               ▼
       ┌─────────────────┐
       │Subzone Creation │
       └───────┬─────────┘
               │
               ▼
     ┌─────────────────────┐
     │ Property Assignment │
     └─────────┬───────────┘
               │
               ▼
┌───────────────────────────────┐
│          Serialization        │
└──────┬───────────────┬────────┘
       │               │
       ▼               ▼
 ┌────────────┐  ┌─────────────┐
 │GeoJSON     │  │ GeoTIFF     │
 │Export      │  │ Export      │
 └─────┬──────┘  └──────┬──────┘
       │                │
       ▼                ▼
┌──────────────┐ ┌──────────────┐
│Vector Data   │ │Raster Data   │
│• Borders     │ │• Elevation   │
│• Subzones    │ │• Soil        │
│• Metadata    │ │• Sensors     │
└──────┬───────┘ └──────┬───────┘
       │                │
       └────────┬───────┘
                ▼
      ┌─────────────────┐
      │  Zone Manager   │
      └─────────┬───────┘
                │
                ▼
      ┌─────────────────┐
      │Robot Coordination│
      └─────────┬───────┘
                │
                ▼
      ┌─────────────────┐
      │ Task Assignment │
      └─────────┬───────┘
                │
                ▼
      ┌─────────────────┐
      │Conflict Resolution│
      └─────────────────┘
```

### Distributed Coordination Model

```
Robot 1      Robot 2      Zone Manager        Zone
   │            │               │             │
   │            │               │             │
   ├─Request Zone Access───────►│             │
   │            │               │             │
   │            │               ├─Check──────►│
   │            │               │◄─Available──┤
   │            │               │             │
   │◄─Grant Access──────────────┤             │
   │ (UUID + Timestamp)         │             │
   │            │               │             │
   │            ├─Request Same Zone──────────►│
   │            │               │             │
   │            │               ├─Check──────►│
   │            │               │◄─Occupied───┤
   │            │               │  by R1      │
   │            │               │             │
   │            │◄─Deny Access──┤             │
   │            │ (Suggest Alt.)│             │
   │            │               │             │
   ├─Release Zone──────────────►│             │
   │            │               │             │
   │            │               ├─Mark───────►│
   │            │               │ Available   │
   │            │               │             │
   │            │◄─Zone Now Available─────────┤
   │            │               │             │
```

### Spatial Reasoning System

```
Point Query ──► Inside Zone? ─┬─ Yes ──► Check Subzones ──► Inside Subzone? ─┬─ Yes ──► Return Subzone Info
                              │                                              │
                              └─ No ───► Find Nearest Zone                   └─ No ───► Return Zone Info

Layer Sampling ──► Bilinear Interpolation ──► Return Sensor Value

Buffer Operations ──► Polygon Expansion ──► Safety Margins

Spatial Indexing ──► R-tree Queries ──► Fast Spatial Lookups

┌─────────────────────────────────────────────────────────────────┐
│                    Spatial Query Processing                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Point-in-Polygon Test                                       │
│     ┌─────────┐     ┌─────────────┐     ┌─────────────────┐     │
│     │ Query   │────►│Ray Casting  │────►│ Inside/Outside  │     │
│     │ Point   │     │ Algorithm   │     │ Determination   │     │
│     └─────────┘     └─────────────┘     └─────────────────┘     │
│                                                                 │
│  2. Nearest Neighbor Search                                     │
│     ┌─────────┐     ┌─────────────┐     ┌─────────────────┐     │
│     │Distance │────►│R-tree Index │────►│ Closest Zone    │     │
│     │Calc     │     │ Search      │     │ Identification  │     │
│     └─────────┘     └─────────────┘     └─────────────────┘     │
│                                                                 │
│  3. Layer Value Interpolation                                   │
│     ┌─────────┐     ┌─────────────┐     ┌─────────────────┐     │
│     │Grid     │────►│Bilinear     │────►│ Interpolated    │     │
│     │Lookup   │     │Interpolation│     │ Sensor Value    │     │
│     └─────────┘     └─────────────┘     └─────────────────┘     │
└─────────────────────────────────────────────────────────────────┘
```

### Zone Types and Use Cases

#### Agricultural Zones (Flexible String Types)
- **"field"**: Crop management, harvest planning, soil analysis
- **"barn"**: Livestock management, milking operations, feeding  
- **"storage"**: Equipment storage, feed storage, maintenance areas
- **"road"**: Navigation paths, access control, traffic management
- **"greenhouse"**: Controlled environment agriculture
- **"silo"**: Grain and feed storage
- **"maintenance_shed"**: Equipment repair and storage
- **Custom types**: Any string can define zone purpose (e.g., "processing_plant", "quarantine_area")

#### Zone Layers (Raster Data)
- **Elevation**: Terrain analysis, drainage planning
- **Soil Properties**: pH, moisture, nutrients, compaction
- **Crop Health**: NDVI, disease detection, growth monitoring
- **Environmental**: Temperature, humidity, weather patterns

#### Subzones (Fine-grained Areas - Flexible String Types)
- **"crop_row"**: Individual planting rows, harvesting paths
- **"irrigation_block"**: Sprinkler coverage, water management
- **"milking_station"**: Individual cow positions, queue management
- **"feeding_area"**: Automated feeding locations, portion control
- **"planting_section"**: Specific crop variety areas
- **"work_area"**: Temporary operational zones
- **"restricted_area"**: No-access safety zones
- **Custom types**: Any string can define subzone purpose (e.g., "hydroponic_section", "sorting_area")

### Data Flow and Persistence

```
                           Zone Creation
                                │
                                ▼
                        In-Memory Objects
                                │
                                ▼
                        Zone Serialization
                           ┌────┴────┐
                           ▼         ▼
                   Vector Data    Raster Data
                      Export        Export
                           │            │
                           ▼            ▼
               ┌─────────────────┐  ┌─────────────────┐
               │  .geojson File  │  │   .tiff File    │
               │ • Borders       │  │ • Sensor Layers │
               │ • Metadata      │  │ • Elevation     │
               │ • Subzones      │  │ • Environment   │
               └─────────────────┘  └─────────────────┘
                           │            │
                           └─────┬──────┘
                                 │
                    ┌─────────────────────────┐
                    │       PERSISTENCE       │
                    │      (File System)      │
                    └─────────────────────────┘
                                 │
                                 ▼
                            Zone Loading
                                 │
                                 ▼
                           File Discovery
                                 │
                           ┌─────┴─────┐
                           ▼           ▼
                  Deserialize      Deserialize
                  Vector Data      Raster Data
                           │           │
                           └─────┬─────┘
                                 ▼
                     Reconstruct Zone Objects
                                 │
                                 ▼
                         Spatial Indexing
                                 │
                                 ▼
                        Ready for Queries

┌──────────────────────────────────────────────────────────────────┐
│                        File Format Details                       │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  GeoJSON Structure:                                              │
│  {                                                               │
│    "type": "FeatureCollection",                                  │
│    "features": [                                                 │
│      {                                                           │
│        "type": "Feature",                                        │
│        "properties": {                                           │
│          "id": "uuid-string",                                    │
│          "name": "Field A",                                      │
│          "type": "field",                                        │
│          "feature_type": "zone_border"                           │
│        },                                                        │
│        "geometry": { "type": "Polygon", "coordinates": [...] }   │
│      }                                                           │
│    ]                                                             │
│  }                                                               │
│                                                                  │
│  GeoTIFF Structure:                                              │
│  • Multi-band raster data                                        │
│  • Georeferenced coordinate system                               │
│  • Layer metadata in TIFF tags                                   │
│  • Spatial resolution information                                │
└──────────────────────────────────────────────────────────────────┘
```

### Time-based Coordination

```
Zone Access Timeline
════════════════════════════════════════════════════════════════════

Time:     0    50   100   150   200   250   300   350   400
          │    │    │     │     │     │     │     │     │
Robot 1:  ├────────Zone A Access────────┤         │     │
          │    │    │     ├─────────Zone B Access─────┤ │
          │    │    │     │     │     │     │     │     │
Robot 2:  ├─────────────Zone C Access───┤   │     │     │
          │    │    ┌─────┴─Waiting─┴───┐         │     │
          │    │    │     │     │     ├─Zone B Access─┤ │
          │    │    │     │     │     │     │     │     │
Robot 3:  │    ┌─Waiting──┐     │     │     │     │     │
          │    │    ├─────────Zone A Access─────────┤   │
          │    │    │     │     │     │     │     │     │

  
Zone Conflict Resolution:
┌─────────────────────────────────────────────────────────────────┐
│                    Temporal Access Control                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Zone Request Processing                                     │
│     Request Time: T1                                            │
│     Lamport Clock: LC(T1)                                       │
│     Robot Priority: UUID-based                                  │
│                                                                 │
│  2. Conflict Detection                                          │
│     IF zone.occupied AND requester != current_owner:           │
│       - Calculate remaining time                                │
│       - Suggest alternative zones                               │
│       - Queue request with timestamp                            │
│                                                                 │
│  3. Access Granting                                             │
│     zone.owner = robot_uuid                                     │
│     zone.start_time = current_timestamp                         │
│     zone.estimated_duration = task_duration                     │
│                                                                 │
│  4. Release Protocol                                            │
│     zone.owner = null                                           │
│     zone.available_time = release_timestamp                     │
│     notify_waiting_robots(zone_id)                              │
└─────────────────────────────────────────────────────────────────┘
```

### Integration with ARIS Framework

Zoneout implements concepts from the ARIS (Autonomous Robot Interaction System) paper for decentralized coordination:

- **No Central Coordinator**: Each robot maintains local zone knowledge
- **Lamport Clocks**: Distributed timestamp synchronization
- **Conflict Resolution**: Negotiation protocols for zone access
- **Spatial Partitioning**: Efficient workspace division
- **Real-time Updates**: Dynamic zone state management

---
