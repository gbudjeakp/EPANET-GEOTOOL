# CMakeLists.txt - CMake configuration for EPANET-Geo library
#
# This library provides geospatial capabilities for EPANET including:
# - Coordinate Reference System (CRS) support
# - Import/export of spatial data formats (Shapefile, GeoJSON, GeoPackage)
# - Terrain integration (DEM elevation assignment)
# - Spatial queries
#
# Dependencies: GDAL (includes OGR), PROJ, optionally GEOS

cmake_minimum_required(VERSION 3.14)

# Define the geo library source files
set(EPANET_GEO_SOURCES
    src/geo_context.c
    src/geo_crs.c
    src/geo_import.c
    src/geo_export.c
    src/geo_terrain.c
    src/geo_spatial.c
    src/geo_util.c
)

set(EPANET_GEO_HEADERS
    include/epanet_geo.h
    include/epanet_geo_enums.h
)

# Find required dependencies
find_package(GDAL QUIET)
find_package(PROJ QUIET)

# Check if dependencies are available
if(NOT GDAL_FOUND)
    message(STATUS "GDAL not found - epanet-geo will not be built")
    message(STATUS "Install GDAL (e.g., 'brew install gdal' on macOS, 'apt install libgdal-dev' on Ubuntu)")
    return()
endif()

if(NOT PROJ_FOUND)
    # Try pkg-config as fallback for PROJ
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PROJ QUIET proj)
    endif()
    
    if(NOT PROJ_FOUND)
        message(STATUS "PROJ not found - epanet-geo will not be built")
        message(STATUS "Install PROJ (e.g., 'brew install proj' on macOS, 'apt install libproj-dev' on Ubuntu)")
        return()
    endif()
endif()

message(STATUS "Building epanet-geo with GDAL ${GDAL_VERSION} and PROJ")

# Create the geo library
if(BUILD_SHARED_LIBS)
    add_library(epanet-geo SHARED ${EPANET_GEO_SOURCES})
    target_compile_definitions(epanet-geo PRIVATE epanet_geo_EXPORTS)
else()
    add_library(epanet-geo STATIC ${EPANET_GEO_SOURCES})
endif()

# Include directories
target_include_directories(epanet-geo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${GDAL_INCLUDE_DIRS}
)

# Link dependencies
target_link_libraries(epanet-geo
    PUBLIC
        epanet2
    PRIVATE
        ${GDAL_LIBRARIES}
)

# Add PROJ include and link
if(PROJ_INCLUDE_DIRS)
    target_include_directories(epanet-geo PRIVATE ${PROJ_INCLUDE_DIRS})
endif()
if(PROJ_LIBRARIES)
    target_link_libraries(epanet-geo PRIVATE ${PROJ_LIBRARIES})
elseif(PROJ_LINK_LIBRARIES)
    target_link_libraries(epanet-geo PRIVATE ${PROJ_LINK_LIBRARIES})
else()
    # Try to find PROJ library manually
    find_library(PROJ_LIBRARY NAMES proj proj_d)
    if(PROJ_LIBRARY)
        target_link_libraries(epanet-geo PRIVATE ${PROJ_LIBRARY})
    endif()
endif()

# Optional GEOS support for advanced spatial queries
find_package(GEOS QUIET)
if(GEOS_FOUND)
    message(STATUS "GEOS found - enabling advanced spatial queries")
    target_compile_definitions(epanet-geo PRIVATE ENGEO_HAS_GEOS)
    target_include_directories(epanet-geo PRIVATE ${GEOS_INCLUDE_DIRS})
    target_link_libraries(epanet-geo PRIVATE ${GEOS_LIBRARIES})
else()
    message(STATUS "GEOS not found - basic spatial queries only")
endif()

# Set library properties
set_target_properties(epanet-geo PROPERTIES
    PUBLIC_HEADER "${EPANET_GEO_HEADERS}"
    DEBUG_POSTFIX "d"
)

# Platform-specific settings
if(MSVC)
    target_compile_definitions(epanet-geo PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(epanet-geo PROPERTIES
        COMPILE_FLAGS "/GL"
    )
endif()

if(APPLE)
    set_target_properties(epanet-geo PROPERTIES
        INSTALL_NAME_DIR @executable_path/../lib
    )
endif()

# Generate export header for symbol visibility
include(GenerateExportHeader)
generate_export_header(epanet-geo
    BASE_NAME epanet_geo
    EXPORT_MACRO_NAME EPANET_GEO_EXPORT
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/epanet_geo_export.h
)

target_include_directories(epanet-geo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Install targets
install(TARGETS epanet-geo
    EXPORT epanet-geo-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/epanet_geo_export.h
    DESTINATION include
)
