#!/bin/bash
# =============================================================================
# EPANET GeoTools - One-Click Setup Script
# =============================================================================
# This script sets up everything needed for the EPANET QGIS plugin:
#   - Builds the EPANET and epanet-geo libraries
#   - Installs the plugin to QGIS
#   - Generates test data (DEM, GeoJSON)
#   - Provides instructions for use
#
# Usage: ./setup.sh [options]
#   --skip-build    Skip building libraries (use existing)
#   --skip-install  Skip plugin installation
#   --skip-data     Skip test data generation
#   --clean         Clean build before rebuilding
# =============================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EPANET_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$EPANET_DIR/build"
PLUGIN_DIR="$SCRIPT_DIR"
TEST_DATA_DIR="$PLUGIN_DIR/test_data"

# QGIS plugin directories (try multiple locations)
QGIS_PLUGIN_DIRS=(
    "$HOME/Library/Application Support/QGIS/QGIS4/profiles/default/python/plugins"
    "$HOME/Library/Application Support/QGIS/QGIS3/profiles/default/python/plugins"
    "$HOME/.local/share/QGIS/QGIS3/profiles/default/python/plugins"
    "$HOME/.qgis2/python/plugins"
)

# Parse arguments
SKIP_BUILD=false
SKIP_INSTALL=false
SKIP_DATA=false
CLEAN_BUILD=false

for arg in "$@"; do
    case $arg in
        --skip-build)
            SKIP_BUILD=true
            ;;
        --skip-install)
            SKIP_INSTALL=true
            ;;
        --skip-data)
            SKIP_DATA=true
            ;;
        --clean)
            CLEAN_BUILD=true
            ;;
        --help|-h)
            echo "EPANET GeoTools Setup Script"
            echo ""
            echo "Usage: ./setup.sh [options]"
            echo ""
            echo "Options:"
            echo "  --skip-build    Skip building libraries"
            echo "  --skip-install  Skip plugin installation"
            echo "  --skip-data     Skip test data generation"
            echo "  --clean         Clean build before rebuilding"
            echo "  --help, -h      Show this help"
            exit 0
            ;;
    esac
done

# -----------------------------------------------------------------------------
# Helper functions
# -----------------------------------------------------------------------------

print_header() {
    echo ""
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

print_step() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 is not installed"
        return 1
    fi
    return 0
}

# -----------------------------------------------------------------------------
# Main setup
# -----------------------------------------------------------------------------

print_header "EPANET GeoTools - One-Click Setup"

echo "EPANET directory: $EPANET_DIR"
echo "Plugin directory: $PLUGIN_DIR"
echo ""

# -----------------------------------------------------------------------------
# Step 1: Check prerequisites
# -----------------------------------------------------------------------------

print_header "Step 1: Checking Prerequisites"

MISSING_DEPS=false

# Check CMake
if check_command cmake; then
    print_step "CMake found: $(cmake --version | head -1)"
else
    MISSING_DEPS=true
fi

# Check compiler
if check_command clang || check_command gcc; then
    print_step "C compiler found"
else
    print_warning "No C compiler found - install Xcode Command Line Tools"
    MISSING_DEPS=true
fi

# Check Python
if check_command python3; then
    print_step "Python found: $(python3 --version)"
else
    MISSING_DEPS=true
fi

# Check QGIS
QGIS_APP=""
if [ -d "/Applications/QGIS-final-4_2_0.app" ]; then
    QGIS_APP="/Applications/QGIS-final-4_2_0.app"
elif [ -d "/Applications/QGIS.app" ]; then
    QGIS_APP="/Applications/QGIS.app"
elif [ -d "/Applications/QGIS-LTR.app" ]; then
    QGIS_APP="/Applications/QGIS-LTR.app"
fi

if [ -n "$QGIS_APP" ]; then
    print_step "QGIS found: $QGIS_APP"
else
    print_warning "QGIS not found in /Applications"
    print_warning "Please install QGIS from https://qgis.org/download/"
fi

# Find QGIS plugin directory
QGIS_PLUGIN_DIR=""
for dir in "${QGIS_PLUGIN_DIRS[@]}"; do
    if [ -d "$(dirname "$dir")" ]; then
        QGIS_PLUGIN_DIR="$dir"
        break
    fi
done

if [ -n "$QGIS_PLUGIN_DIR" ]; then
    print_step "QGIS plugin directory: $QGIS_PLUGIN_DIR"
else
    print_warning "QGIS plugin directory not found"
    print_warning "Will try to create it during installation"
fi

if $MISSING_DEPS; then
    print_error "Missing dependencies. Please install them and try again."
    exit 1
fi

# -----------------------------------------------------------------------------
# Step 2: Build libraries
# -----------------------------------------------------------------------------

if ! $SKIP_BUILD; then
    print_header "Step 2: Building EPANET Libraries"
    
    # Clean if requested
    if $CLEAN_BUILD && [ -d "$BUILD_DIR" ]; then
        print_step "Cleaning previous build..."
        rm -rf "$BUILD_DIR"
    fi
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_step "Configuring with CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF \
        -DBUILD_GEO=ON \
        2>&1 | tail -5
    
    # Build
    print_step "Building libraries..."
    cmake --build . --config Release -j$(sysctl -n hw.ncpu) 2>&1 | tail -10
    
    # Check for libraries
    if [ -f "$BUILD_DIR/lib/libepanet2.dylib" ] || [ -f "$BUILD_DIR/lib/libepanet2.so" ]; then
        print_step "Built: libepanet2"
    else
        print_warning "libepanet2 not found - checking alternative locations..."
    fi
    
    if [ -f "$BUILD_DIR/lib/libepanet-geo.dylib" ] || [ -f "$BUILD_DIR/lib/libepanet-geo.so" ]; then
        print_step "Built: libepanet-geo"
    else
        print_warning "libepanet-geo not found"
    fi
    
    cd "$SCRIPT_DIR"
else
    print_header "Step 2: Skipping Build (--skip-build)"
fi

# -----------------------------------------------------------------------------
# Step 3: Install plugin
# -----------------------------------------------------------------------------

if ! $SKIP_INSTALL; then
    print_header "Step 3: Installing QGIS Plugin"
    
    # Create plugin directory if needed
    if [ -z "$QGIS_PLUGIN_DIR" ]; then
        QGIS_PLUGIN_DIR="${QGIS_PLUGIN_DIRS[0]}"
    fi
    
    mkdir -p "$QGIS_PLUGIN_DIR"
    
    # Plugin destination
    DEST_DIR="$QGIS_PLUGIN_DIR/epanet_geotools"
    
    # Remove old installation
    if [ -d "$DEST_DIR" ]; then
        print_step "Removing previous installation..."
        rm -rf "$DEST_DIR"
    fi
    
    # Create plugin directory
    mkdir -p "$DEST_DIR"
    mkdir -p "$DEST_DIR/lib"
    mkdir -p "$DEST_DIR/test_data"
    
    # Copy plugin files
    print_step "Copying plugin files..."
    cp "$PLUGIN_DIR"/*.py "$DEST_DIR/" 2>/dev/null || true
    cp "$PLUGIN_DIR"/metadata.txt "$DEST_DIR/" 2>/dev/null || true
    
    # Copy libraries (check multiple possible locations)
    print_step "Copying libraries..."
    
    # Library search paths
    LIB_PATHS=(
        "$BUILD_DIR/lib"
        "$BUILD_DIR/src/geo"
        "$BUILD_DIR"
        "$PLUGIN_DIR/lib"
    )
    
    for lib in libepanet2.dylib libepanet2.so libepanet-geo.dylib libepanet-geo.so; do
        for libpath in "${LIB_PATHS[@]}"; do
            if [ -f "$libpath/$lib" ]; then
                cp "$libpath/$lib" "$DEST_DIR/lib/"
                print_step "  Copied: $lib (from $libpath)"
                break
            fi
        done
    done
    
    # Copy test data
    if [ -d "$TEST_DATA_DIR" ]; then
        print_step "Copying test data..."
        cp "$TEST_DATA_DIR"/*.asc "$DEST_DIR/test_data/" 2>/dev/null || true
        cp "$TEST_DATA_DIR"/*.geojson "$DEST_DIR/test_data/" 2>/dev/null || true
        cp "$TEST_DATA_DIR"/*.qgs "$DEST_DIR/test_data/" 2>/dev/null || true
    fi
    
    # Copy example networks
    print_step "Copying example networks..."
    cp "$EPANET_DIR/example-networks"/*.inp "$DEST_DIR/" 2>/dev/null || true
    
    print_step "Plugin installed to: $DEST_DIR"
else
    print_header "Step 3: Skipping Install (--skip-install)"
fi

# -----------------------------------------------------------------------------
# Step 4: Generate test data
# -----------------------------------------------------------------------------

if ! $SKIP_DATA; then
    print_header "Step 4: Generating Test Data"
    
    cd "$TEST_DATA_DIR"
    
    if [ -f "generate_test_data.py" ]; then
        python3 generate_test_data.py
        print_step "Test data generated"
    elif [ -f "generate_test_data_simple.py" ]; then
        python3 generate_test_data_simple.py
        print_step "Test data generated"
    else
        print_warning "Test data generator not found"
    fi
    
    cd "$SCRIPT_DIR"
else
    print_header "Step 4: Skipping Test Data (--skip-data)"
fi

# -----------------------------------------------------------------------------
# Done!
# -----------------------------------------------------------------------------

print_header "Setup Complete!"

echo ""
echo -e "${GREEN}EPANET GeoTools has been set up successfully!${NC}"
echo ""
echo "Next steps:"
echo ""
echo "  1. Open QGIS"
echo ""
echo "  2. Enable the plugin:"
echo "     Plugins → Manage and Install Plugins → Installed"
echo "     Find 'EPANET GeoTools' and check the box"
echo ""
echo "  3. Use the plugin:"
echo "     - Look for the EPANET toolbar"
echo "     - Or use Plugins → EPANET GeoTools menu"
echo ""
echo "  4. Try with test data:"
echo "     - Open File → Open INP File"
echo "     - Select: $DEST_DIR/Net1.inp"
echo "     - Or load test data from: $DEST_DIR/test_data/"
echo ""
echo "Test data files:"
echo "  - net1_dem.asc       (synthetic terrain for DEM testing)"
echo "  - net1_nodes.geojson (network nodes for import testing)"
echo "  - net1_pipes.geojson (network pipes for import testing)"
echo ""

if [ -n "$QGIS_APP" ]; then
    echo -e "Quick start: ${BLUE}open \"$QGIS_APP\"${NC}"
fi
