# EPANET-GEOTOOL

**A geospatial extension for EPANET water distribution network modeling.**

EPANET-GEOTOOL extends the [EPANET](https://github.com/OpenWaterAnalytics/EPANET) hydraulic modeling engine with full GIS capabilities, including a QGIS plugin for visual network editing and analysis.

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

## 🌟 Features

### epanet-geo Library (C)
- **Coordinate Reference System (CRS) support** - Work with real-world coordinates (EPSG codes)
- **Coordinate transformations** - Convert between projections using PROJ
- **DEM integration** - Assign node elevations from terrain rasters (GeoTIFF, ASCII Grid)
- **GIS import/export** - Read/write Shapefiles, GeoJSON, GeoPackage via GDAL
- **Spatial indexing** - Fast spatial queries for large networks
- **Automatic pipe length calculation** - From geographic coordinates

### QGIS Plugin
- **Visual network editing** - Create and modify networks directly on the map
- **One-click simulation** - Run EPANET from within QGIS
- **Result visualization** - Automatic pressure/flow symbology
- **Import from GIS** - Convert existing infrastructure data to EPANET
- **Export to GIS** - Share results with other GIS software
- **DEM elevation assignment** - Extract elevations from terrain data

---

## 🚀 Quick Start

### One-Click Setup (macOS/Linux)

```bash
git clone https://github.com/gbudjeakp/EPANET-GEOTOOL.git
cd EPANET-GEOTOOL/qgis_plugin
./setup.sh
```

This will:
1. Build the EPANET and epanet-geo libraries
2. Install the QGIS plugin
3. Generate test data (DEM, sample networks)

### Manual Build

```bash
mkdir build && cd build
cmake -DBUILD_GEO=ON ..
cmake --build .
```

---

## 📦 Components

```
EPANET-GEOTOOL/
├── src/                    # Core EPANET source
│   └── geo/                # ⭐ epanet-geo library (NEW)
│       ├── include/        # Public API headers
│       └── src/            # Implementation
├── qgis_plugin/            # ⭐ QGIS Plugin (NEW)
│   ├── epanet_bindings.py  # Python bindings
│   ├── epanet_geo_plugin.py
│   ├── dialogs.py
│   └── test_data/          # Sample DEM and GeoJSON
├── include/                # EPANET public headers
└── example-networks/       # Sample .inp files
```

---

## 🔧 Requirements

### For Building
- CMake 3.10+
- C compiler (GCC, Clang, MSVC)
- GDAL (for GIS support)
- PROJ (for coordinate transformations)

### For QGIS Plugin
- QGIS 3.22+ or 4.x
- Python 3.8+

### Install Dependencies

**macOS (Homebrew):**
```bash
brew install cmake gdal proj
```

**Ubuntu/Debian:**
```bash
sudo apt install cmake libgdal-dev libproj-dev
```

**Windows:**
Use OSGeo4W or vcpkg to install GDAL and PROJ.

---

## 📖 Usage

### QGIS Plugin

1. **Open QGIS** and enable the plugin (Plugins → Manage and Install Plugins)
2. **Open an INP file** or create a new project
3. **Import GIS data** - Load existing shapefiles/GeoJSON
4. **Assign elevations** - Use DEM terrain data
5. **Run simulation** - One-click hydraulic analysis
6. **View results** - Pressure and flow displayed on the map

### epanet-geo Library (C API)

```c
#include "epanet2_2.h"
#include "epanet_geo.h"

// Create EPANET project
EN_Project ph;
EN_createproject(&ph);
EN_open(ph, "network.inp", "report.rpt", "");

// Create geo context and attach
ENGEO_Handle gh;
ENGEO_create(&gh);
ENGEO_attach(gh, ph);

// Set coordinate system
ENGEO_setcrs_epsg(gh, 32614);  // UTM Zone 14N

// Assign elevations from DEM
ENGEO_open_dem(gh, "terrain.tif");
ENGEO_assign_elevations(gh, ENGEO_INTERP_BILINEAR);
ENGEO_close_dem(gh);

// Run simulation
EN_openH(ph);
EN_initH(ph, EN_NOSAVE);
EN_runH(ph, &t);
EN_closeH(ph);

// Export to GeoJSON
ENGEO_export_geojson(gh, "output.geojson", ENGEO_EXPORT_ALL);

// Cleanup
ENGEO_destroy(&gh);
EN_close(ph);
EN_deleteproject(ph);
```

---

## 🆚 Comparison with Commercial Tools

| Feature | InfoWater Pro | WaterGEMS | EPANET-GEOTOOL |
|---------|--------------|-----------|----------------|
| **Cost** | $5,000+/year | $3,000+/year | **Free** |
| **GIS Integration** | ArcGIS only | Limited | **QGIS (free)** |
| **Platform** | Windows | Windows | **All platforms** |
| **Open Source** | No | No | **Yes (MIT)** |
| **DEM Support** | Yes | Yes | **Yes** |
| **CRS Support** | Via ArcGIS | Limited | **Full PROJ** |

---

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```bash
# Clone with submodules
git clone --recursive https://github.com/gbudjeakp/EPANET-GEOTOOL.git

# Build in debug mode
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_GEO=ON -DBUILD_TESTS=ON ..
cmake --build .

# Run tests
ctest
```

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

EPANET is developed by the US Environmental Protection Agency.

---

## 🙏 Acknowledgments

- [OpenWaterAnalytics/EPANET](https://github.com/OpenWaterAnalytics/EPANET) - The original EPANET project
- [US EPA](https://www.epa.gov/water-research/epanet) - EPANET development
- [QGIS](https://qgis.org) - Free and open source GIS
- [GDAL](https://gdal.org) - Geospatial Data Abstraction Library
- [PROJ](https://proj.org) - Coordinate transformation library

---

## 📬 Contact

- **Issues:** [GitHub Issues](https://github.com/gbudjeakp/EPANET-GEOTOOL/issues)
- **Author:** [@gbudjeakp](https://github.com/gbudjeakp)
