/** @file epanet_geo_enums.h
 */
/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       epanet_geo_enums.h
 Description:  enumerations of symbolic constants used by the Geo API functions
 Authors:      see AUTHORS
 Copyright:    see AUTHORS
 License:      see LICENSE
 Last Updated: 07/26/2026
 ******************************************************************************
*/

#ifndef EPANET_GEO_ENUMS_H
#define EPANET_GEO_ENUMS_H

/// Geospatial file format types
typedef enum {
    ENGEO_SHAPEFILE    = 0,  //!< ESRI Shapefile format
    ENGEO_GEOJSON      = 1,  //!< GeoJSON format
    ENGEO_GEOPACKAGE   = 2,  //!< OGC GeoPackage format
    ENGEO_KML          = 3,  //!< Keyhole Markup Language
    ENGEO_GML          = 4   //!< Geography Markup Language
} ENGEO_FileFormat;

/// Layer types for import/export
typedef enum {
    ENGEO_LAYER_JUNCTIONS   = 0,  //!< Junction nodes (point geometry)
    ENGEO_LAYER_RESERVOIRS  = 1,  //!< Reservoir nodes (point geometry)
    ENGEO_LAYER_TANKS       = 2,  //!< Tank nodes (point geometry)
    ENGEO_LAYER_PIPES       = 3,  //!< Pipe links (line geometry)
    ENGEO_LAYER_PUMPS       = 4,  //!< Pump links (line geometry)
    ENGEO_LAYER_VALVES      = 5,  //!< Valve links (line geometry)
    ENGEO_LAYER_ALL_NODES   = 6,  //!< All node types
    ENGEO_LAYER_ALL_LINKS   = 7,  //!< All link types
    ENGEO_LAYER_ALL         = 8   //!< Complete network
} ENGEO_LayerType;

/// Attribute mapping options for import
typedef enum {
    ENGEO_MAP_AUTO     = 0,  //!< Auto-detect attribute mapping
    ENGEO_MAP_MANUAL   = 1,  //!< Use manual attribute mapping
    ENGEO_MAP_NONE     = 2   //!< Import geometry only, no attributes
} ENGEO_MappingMode;

/// CRS definition types
typedef enum {
    ENGEO_CRS_NONE     = 0,  //!< No CRS defined
    ENGEO_CRS_EPSG     = 1,  //!< CRS defined by EPSG code
    ENGEO_CRS_WKT      = 2,  //!< CRS defined by WKT string
    ENGEO_CRS_PROJ     = 3   //!< CRS defined by PROJ string
} ENGEO_CRSType;

/// Spatial query types
typedef enum {
    ENGEO_QUERY_WITHIN     = 0,  //!< Features within distance
    ENGEO_QUERY_INTERSECTS = 1,  //!< Features that intersect
    ENGEO_QUERY_CONTAINS   = 2,  //!< Features that contain
    ENGEO_QUERY_NEAREST    = 3   //!< Nearest feature
} ENGEO_QueryType;

/// Export result options
typedef enum {
    ENGEO_EXPORT_NETWORK_ONLY  = 0,  //!< Export network geometry only
    ENGEO_EXPORT_WITH_RESULTS  = 1,  //!< Export with simulation results
    ENGEO_EXPORT_RESULTS_ONLY  = 2   //!< Export results as separate layer
} ENGEO_ExportMode;

/// Elevation assignment method
typedef enum {
    ENGEO_ELEV_NEAREST   = 0,  //!< Nearest neighbor sampling
    ENGEO_ELEV_BILINEAR  = 1,  //!< Bilinear interpolation
    ENGEO_ELEV_CUBIC     = 2   //!< Cubic interpolation
} ENGEO_ElevMethod;

/// Error codes specific to geo operations
typedef enum {
    ENGEO_OK                  = 0,    //!< No error
    ENGEO_ERR_NOT_INITIALIZED = 501,  //!< Geo context not initialized
    ENGEO_ERR_NO_PROJECT      = 502,  //!< No EPANET project attached
    ENGEO_ERR_FILE_NOT_FOUND  = 503,  //!< Input file not found
    ENGEO_ERR_INVALID_FORMAT  = 504,  //!< Invalid or unsupported file format
    ENGEO_ERR_READ_FAILED     = 505,  //!< Failed to read file
    ENGEO_ERR_WRITE_FAILED    = 506,  //!< Failed to write file
    ENGEO_ERR_NO_CRS          = 507,  //!< No CRS defined
    ENGEO_ERR_CRS_MISMATCH    = 508,  //!< CRS mismatch between datasets
    ENGEO_ERR_TRANSFORM_FAILED= 509,  //!< Coordinate transformation failed
    ENGEO_ERR_NO_GEOMETRY     = 510,  //!< Feature has no geometry
    ENGEO_ERR_INVALID_GEOMETRY= 511,  //!< Invalid geometry type
    ENGEO_ERR_NO_DEM          = 512,  //!< DEM file not found or invalid
    ENGEO_ERR_OUT_OF_BOUNDS   = 513,  //!< Coordinates outside DEM extent
    ENGEO_ERR_GDAL_INIT       = 514,  //!< GDAL initialization failed
    ENGEO_ERR_PROJ_INIT       = 515,  //!< PROJ initialization failed
    ENGEO_ERR_GEOS_INIT       = 516,  //!< GEOS initialization failed
    ENGEO_ERR_OUT_OF_MEMORY   = 517,  //!< Memory allocation failed
    ENGEO_ERR_INVALID_ARG     = 518,  //!< Invalid argument
    ENGEO_ERR_NOT_SUPPORTED   = 519   //!< Operation not supported
} ENGEO_Error;

/// Size limits for geo operations
typedef enum {
    ENGEO_MAX_CRS_WKT    = 4096,  //!< Max length of WKT CRS string
    ENGEO_MAX_PROJ_STR   = 1024,  //!< Max length of PROJ string
    ENGEO_MAX_ATTR_NAME  = 64,    //!< Max length of attribute name
    ENGEO_MAX_PATH       = 1024   //!< Max length of file path
} ENGEO_SizeLimits;

#endif /* EPANET_GEO_ENUMS_H */
