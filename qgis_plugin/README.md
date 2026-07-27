# EPANET GeoTools - QGIS Plugin

A free, open-source GIS interface for EPANET water distribution network modeling.

## Features

- **Import GIS Data**: Load networks from Shapefiles, GeoJSON, or GeoPackage
- **Export to GIS**: Save networks and results in standard GIS formats
- **DEM Integration**: Automatically assign node elevations from terrain rasters
- **Coordinate Systems**: Full CRS support with on-the-fly transformation
- **Hydraulic Simulation**: Run EPANET from within QGIS
- **Result Visualization**: Style layers based on pressure, flow, velocity

## Requirements

- QGIS 3.22+ or QGIS 4.x
- macOS, Linux, or Windows
- CMake 3.10+ (for building)

## Quick Start (One-Click Setup)

```bash
cd qgis_plugin
./setup.sh
```

This will:
1. Build the EPANET and epanet-geo libraries
2. Install the plugin to QGIS
3. Generate test data (DEM, GeoJSON files)
4. Copy example networks

Then open QGIS, enable the plugin, and you're ready!

### Setup Options

```bash
./setup.sh --skip-build    # Skip building (use existing libraries)
./setup.sh --skip-install  # Skip plugin installation
./setup.sh --skip-data     # Skip test data generation
./setup.sh --clean         # Clean build before rebuilding
```

## Manual Installation

### Step 1: Build Libraries
```bash
cd /path/to/EPANET
mkdir build && cd build
cmake -DBUILD_GEO=ON ..
cmake --build .
```

### Step 2: Install Plugin
```bash
# macOS (QGIS 4)
PLUGIN_DIR="$HOME/Library/Application Support/QGIS/QGIS4/profiles/default/python/plugins/epanet_geotools"

# macOS (QGIS 3)
PLUGIN_DIR="$HOME/Library/Application Support/QGIS/QGIS3/profiles/default/python/plugins/epanet_geotools"

# Linux
PLUGIN_DIR="$HOME/.local/share/QGIS/QGIS3/profiles/default/python/plugins/epanet_geotools"

mkdir -p "$PLUGIN_DIR/lib"
cp qgis_plugin/*.py "$PLUGIN_DIR/"
cp qgis_plugin/metadata.txt "$PLUGIN_DIR/"
cp build/lib/libepanet2.* "$PLUGIN_DIR/lib/"
cp build/lib/libepanet-geo.* "$PLUGIN_DIR/lib/"
```

### Step 3: Enable in QGIS
1. Open QGIS
2. Go to Plugins → Manage and Install Plugins → Installed
3. Check "EPANET GeoTools"

## Usage

### Opening an EPANET File
1. Click **Open INP File** in the toolbar
2. Select any .inp file (e.g., Net1.inp from example-networks/)
3. The network appears as QGIS layers

### Creating a New Network
1. Click **New EPANET Project** in the toolbar
2. Use QGIS digitizing tools to add nodes and pipes
3. Or import from existing GIS data

### Importing from Shapefile/GeoJSON
1. Click **Import from GIS**
2. Select your file (.shp, .geojson, .gpkg)
3. Choose layer type (junctions, pipes, etc.)
4. Configure attribute mapping (or use auto-detect)
5. Click OK

### Assigning Elevations from DEM
1. Click **Assign Elevations from DEM**
2. Select a DEM file or loaded raster layer
3. Choose interpolation method (bilinear recommended)
4. Click OK

**Test data included:** Use `test_data/net1_dem.asc` for testing

### Running a Simulation
1. Click **Run Simulation**
2. Configure simulation options
3. Click Run
4. View results in layer attributes and map styling

### Exporting Results
1. Click **Export to GIS**
2. Choose output format
3. Check "Include simulation results"
4. Click OK

## Workflow Example

```
1. Import pipes.shp and junctions.shp
2. Set project CRS (e.g., EPSG:32610 for UTM Zone 10N)
3. Load terrain DEM
4. Click "Assign Elevations from DEM"
5. Click "Calculate Pipe Lengths"
6. Edit demands, roughness, etc. in attribute table
7. Click "Run Simulation"
8. View pressure/flow results on map
9. Export to GeoJSON for web sharing
```

## API Reference

### Python Bindings

```python
from epanet_bindings import EpanetProject, EpanetGeo

# Create project
project = EpanetProject()
project.open('network.inp')

# Attach geo context
geo = EpanetGeo()
geo.create()
geo.attach(project)

# Set CRS
geo.set_crs_epsg(4326)

# Import from shapefile
geo.import_pipes_shp('pipes.shp')

# Assign elevations
geo.open_dem('terrain.tif')
geo.assign_elevations(method=1)  # bilinear
geo.close_dem()

# Calculate pipe lengths
geo.calc_pipe_lengths(update=True)

# Run simulation
project.run_hydraulics()

# Export results
geo.export_geojson('results.geojson', export_mode=1)

# Cleanup
geo.destroy()
project.close()
```

## Contributing

Contributions welcome! See the main EPANET repository for guidelines.

## License

MIT License - same as EPANET

## Credits

- EPANET: US Environmental Protection Agency
- Open Water Analytics community
- QGIS Development Team
