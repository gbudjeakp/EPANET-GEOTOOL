/** @file epanet_geo.h
 @see http://github.com/openwateranalytics/epanet
 */

/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       epanet_geo.h
 Description:  API function declarations for geospatial operations
 Authors:      see AUTHORS
 Copyright:    see AUTHORS
 License:      see LICENSE
 Last Updated: 07/26/2026
 ******************************************************************************

 The EPANET-Geo library provides geospatial capabilities for EPANET including:
 - Coordinate Reference System (CRS) support and transformations
 - Import/export of spatial data formats (Shapefile, GeoJSON, GeoPackage)
 - Terrain integration (DEM elevation assignment)
 - Spatial queries (proximity, buffer, spatial joins)

 This is a companion library that requires linking against both epanet2 and
 spatial libraries (GDAL, PROJ, GEOS).
 */

#ifndef EPANET_GEO_H
#define EPANET_GEO_H

// Platform-specific export macros
#ifndef DLLEXPORT_GEO
  #ifdef _WIN32
    #ifdef epanet_geo_EXPORTS
      #define DLLEXPORT_GEO __declspec(dllexport) __stdcall
    #else
      #define DLLEXPORT_GEO __declspec(dllimport) __stdcall
    #endif
  #elif defined(__CYGWIN__)
    #define DLLEXPORT_GEO __stdcall
  #else
    #define DLLEXPORT_GEO
  #endif
#endif

#include "epanet_geo_enums.h"
#include "epanet2_2.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 @brief Opaque handle for the EPANET-Geo context
 
 A geo context maintains CRS information, spatial indices, and other
 geospatial state associated with an EPANET project.
*/
typedef struct GeoContext *ENGEO_Handle;

/**
 @brief Attribute mapping structure for import operations
*/
typedef struct {
    char id_field[ENGEO_MAX_ATTR_NAME];        //!< Field name for element ID
    char elevation_field[ENGEO_MAX_ATTR_NAME]; //!< Field name for elevation
    char demand_field[ENGEO_MAX_ATTR_NAME];    //!< Field name for demand
    char diameter_field[ENGEO_MAX_ATTR_NAME];  //!< Field name for diameter
    char length_field[ENGEO_MAX_ATTR_NAME];    //!< Field name for length
    char roughness_field[ENGEO_MAX_ATTR_NAME]; //!< Field name for roughness
    char status_field[ENGEO_MAX_ATTR_NAME];    //!< Field name for status
} ENGEO_AttributeMap;

/**
 @brief Bounding box structure
*/
typedef struct {
    double min_x;  //!< Minimum X coordinate
    double min_y;  //!< Minimum Y coordinate
    double max_x;  //!< Maximum X coordinate
    double max_y;  //!< Maximum Y coordinate
} ENGEO_Extent;


/*=============================================================================
    Context Management Functions
=============================================================================*/

/**
 @brief Creates a new EPANET-Geo context.
 @param[out] gh pointer to receive the geo context handle.
 @return an error code.
 
 Creates a new geo context that can be attached to an EPANET project.
 Must be destroyed with ENGEO_destroy() when no longer needed.
*/
int DLLEXPORT_GEO ENGEO_create(ENGEO_Handle *gh);

/**
 @brief Destroys an EPANET-Geo context and frees all resources.
 @param[in,out] gh pointer to geo context handle (set to NULL on return).
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_destroy(ENGEO_Handle *gh);

/**
 @brief Attaches a geo context to an EPANET project.
 @param gh a geo context handle.
 @param ph an EPANET project handle.
 @return an error code.
 
 The geo context will operate on the attached EPANET project's network.
 A geo context can only be attached to one project at a time.
*/
int DLLEXPORT_GEO ENGEO_attach(ENGEO_Handle gh, EN_Project ph);

/**
 @brief Detaches a geo context from its EPANET project.
 @param gh a geo context handle.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_detach(ENGEO_Handle gh);

/**
 @brief Gets the EPANET project attached to a geo context.
 @param gh a geo context handle.
 @param[out] ph pointer to receive the EPANET project handle.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_getproject(ENGEO_Handle gh, EN_Project *ph);


/*=============================================================================
    Coordinate Reference System (CRS) Functions
=============================================================================*/

/**
 @brief Sets the CRS using an EPSG code.
 @param gh a geo context handle.
 @param epsg_code the EPSG code (e.g., 4326 for WGS84, 32610 for UTM 10N).
 @return an error code.
 
 Common EPSG codes:
 - 4326: WGS 84 (latitude/longitude)
 - 3857: Web Mercator
 - 32610-32619: UTM zones 10N-19N (US West to East)
 - 2227-2229: California State Plane zones
*/
int DLLEXPORT_GEO ENGEO_setcrs_epsg(ENGEO_Handle gh, int epsg_code);

/**
 @brief Sets the CRS using a WKT string.
 @param gh a geo context handle.
 @param wkt the Well-Known Text CRS definition.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_setcrs_wkt(ENGEO_Handle gh, const char *wkt);

/**
 @brief Sets the CRS using a PROJ string.
 @param gh a geo context handle.
 @param proj_str the PROJ definition string.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_setcrs_proj(ENGEO_Handle gh, const char *proj_str);

/**
 @brief Gets the current CRS type.
 @param gh a geo context handle.
 @param[out] crs_type the CRS type (see ENGEO_CRSType).
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_getcrs_type(ENGEO_Handle gh, int *crs_type);

/**
 @brief Gets the current CRS as an EPSG code.
 @param gh a geo context handle.
 @param[out] epsg_code the EPSG code (0 if not an EPSG-based CRS).
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_getcrs_epsg(ENGEO_Handle gh, int *epsg_code);

/**
 @brief Gets the current CRS as a WKT string.
 @param gh a geo context handle.
 @param[out] wkt buffer to receive the WKT string.
 @param max_len maximum length of the buffer.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_getcrs_wkt(ENGEO_Handle gh, char *wkt, int max_len);

/**
 @brief Transforms all network coordinates to a new CRS.
 @param gh a geo context handle.
 @param target_epsg the target EPSG code.
 @return an error code.
 
 Transforms all node coordinates and link vertices from the current CRS
 to the target CRS. The geo context's CRS is updated to the target.
*/
int DLLEXPORT_GEO ENGEO_transform_network(ENGEO_Handle gh, int target_epsg);

/**
 @brief Transforms a single coordinate pair.
 @param gh a geo context handle.
 @param target_epsg the target EPSG code.
 @param[in,out] x pointer to X coordinate (transformed in place).
 @param[in,out] y pointer to Y coordinate (transformed in place).
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_transform_point(ENGEO_Handle gh, int target_epsg,
                                        double *x, double *y);


/*=============================================================================
    Import Functions
=============================================================================*/

/**
 @brief Imports network data from a geospatial file.
 @param gh a geo context handle.
 @param filepath path to the input file.
 @param format the file format (see ENGEO_FileFormat).
 @param layer_type the type of layer to import (see ENGEO_LayerType).
 @return an error code.
 
 For point geometry files, features are imported as nodes.
 For line geometry files, features are imported as pipes.
 Attributes are auto-mapped to EPANET properties when possible.
*/
int DLLEXPORT_GEO ENGEO_import(ENGEO_Handle gh, const char *filepath,
                               int format, int layer_type);

/**
 @brief Imports with custom attribute mapping.
 @param gh a geo context handle.
 @param filepath path to the input file.
 @param format the file format.
 @param layer_type the type of layer to import.
 @param attr_map attribute mapping configuration.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_import_mapped(ENGEO_Handle gh, const char *filepath,
                                      int format, int layer_type,
                                      const ENGEO_AttributeMap *attr_map);

/**
 @brief Imports nodes from a Shapefile.
 @param gh a geo context handle.
 @param filepath path to the .shp file.
 @param node_type type of nodes to create (ENGEO_LAYER_JUNCTIONS, etc.).
 @return an error code.
 
 Convenience function for importing point shapefiles as network nodes.
*/
int DLLEXPORT_GEO ENGEO_import_nodes_shp(ENGEO_Handle gh, const char *filepath,
                                         int node_type);

/**
 @brief Imports pipes from a Shapefile.
 @param gh a geo context handle.
 @param filepath path to the .shp file.
 @return an error code.
 
 Convenience function for importing line shapefiles as network pipes.
 Line vertices are preserved as pipe vertices.
*/
int DLLEXPORT_GEO ENGEO_import_pipes_shp(ENGEO_Handle gh, const char *filepath);

/**
 @brief Imports a complete network from GeoJSON.
 @param gh a geo context handle.
 @param filepath path to the .geojson file.
 @return an error code.
 
 GeoJSON should contain FeatureCollections for nodes and links with
 a "type" property indicating the element type.
*/
int DLLEXPORT_GEO ENGEO_import_geojson(ENGEO_Handle gh, const char *filepath);

/**
 @brief Imports a complete network from a GeoPackage.
 @param gh a geo context handle.
 @param filepath path to the .gpkg file.
 @return an error code.
 
 The GeoPackage should contain tables named "junctions", "reservoirs",
 "tanks", "pipes", "pumps", "valves" with appropriate geometry types.
*/
int DLLEXPORT_GEO ENGEO_import_geopackage(ENGEO_Handle gh, const char *filepath);


/*=============================================================================
    Export Functions
=============================================================================*/

/**
 @brief Exports network data to a geospatial file.
 @param gh a geo context handle.
 @param filepath path to the output file.
 @param format the file format (see ENGEO_FileFormat).
 @param layer_type the type of layer to export (see ENGEO_LayerType).
 @param mode export mode (see ENGEO_ExportMode).
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_export(ENGEO_Handle gh, const char *filepath,
                               int format, int layer_type, int mode);

/**
 @brief Exports network nodes to a Shapefile.
 @param gh a geo context handle.
 @param filepath path to the output .shp file.
 @param node_type type of nodes to export (or ENGEO_LAYER_ALL_NODES).
 @param include_results whether to include simulation results as attributes.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_export_nodes_shp(ENGEO_Handle gh, const char *filepath,
                                         int node_type, int include_results);

/**
 @brief Exports network links to a Shapefile.
 @param gh a geo context handle.
 @param filepath path to the output .shp file.
 @param link_type type of links to export (or ENGEO_LAYER_ALL_LINKS).
 @param include_results whether to include simulation results as attributes.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_export_links_shp(ENGEO_Handle gh, const char *filepath,
                                         int link_type, int include_results);

/**
 @brief Exports complete network to GeoJSON.
 @param gh a geo context handle.
 @param filepath path to the output .geojson file.
 @param include_results whether to include simulation results as properties.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_export_geojson(ENGEO_Handle gh, const char *filepath,
                                       int include_results);

/**
 @brief Exports complete network to a GeoPackage.
 @param gh a geo context handle.
 @param filepath path to the output .gpkg file.
 @param include_results whether to include simulation results.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_export_geopackage(ENGEO_Handle gh, const char *filepath,
                                          int include_results);


/*=============================================================================
    Terrain Integration Functions
=============================================================================*/

/**
 @brief Opens a DEM raster file for elevation queries.
 @param gh a geo context handle.
 @param filepath path to the DEM file (GeoTIFF, etc.).
 @return an error code.
 
 The DEM remains open until ENGEO_close_dem() is called or the context
 is destroyed. CRS transformation is handled automatically if the DEM
 has a different CRS than the network.
*/
int DLLEXPORT_GEO ENGEO_open_dem(ENGEO_Handle gh, const char *filepath);

/**
 @brief Closes the currently open DEM file.
 @param gh a geo context handle.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_close_dem(ENGEO_Handle gh);

/**
 @brief Assigns elevations to all network nodes from the DEM.
 @param gh a geo context handle.
 @param method the interpolation method (see ENGEO_ElevMethod).
 @return an error code.
 
 Updates the elevation property of all junction, reservoir, and tank nodes
 by sampling the DEM at their coordinates.
*/
int DLLEXPORT_GEO ENGEO_assign_elevations(ENGEO_Handle gh, int method);

/**
 @brief Assigns elevation to a single node from the DEM.
 @param gh a geo context handle.
 @param node_index the node index (1-based).
 @param method the interpolation method.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_assign_node_elevation(ENGEO_Handle gh, int node_index,
                                              int method);

/**
 @brief Samples DEM elevation at a coordinate.
 @param gh a geo context handle.
 @param x the X coordinate.
 @param y the Y coordinate.
 @param method the interpolation method.
 @param[out] elevation the sampled elevation value.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_sample_elevation(ENGEO_Handle gh, double x, double y,
                                         int method, double *elevation);

/**
 @brief Gets elevation profile along a pipe.
 @param gh a geo context handle.
 @param link_index the link index (1-based).
 @param num_samples number of sample points along the pipe.
 @param[out] distances array to receive distance values (size = num_samples).
 @param[out] elevations array to receive elevation values (size = num_samples).
 @return an error code.
 
 Returns evenly-spaced elevation samples along the pipe's path,
 including start node, vertices, and end node.
*/
int DLLEXPORT_GEO ENGEO_get_pipe_profile(ENGEO_Handle gh, int link_index,
                                         int num_samples, double *distances,
                                         double *elevations);


/*=============================================================================
    Spatial Query Functions
=============================================================================*/

/**
 @brief Builds a spatial index for fast queries.
 @param gh a geo context handle.
 @return an error code.
 
 Creates an R-tree spatial index of all network nodes and links.
 Call this before performing multiple spatial queries for best performance.
*/
int DLLEXPORT_GEO ENGEO_build_spatial_index(ENGEO_Handle gh);

/**
 @brief Clears the spatial index.
 @param gh a geo context handle.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_clear_spatial_index(ENGEO_Handle gh);

/**
 @brief Finds nodes within a distance of a point.
 @param gh a geo context handle.
 @param x the X coordinate of the search point.
 @param y the Y coordinate of the search point.
 @param radius the search radius (in CRS units).
 @param[out] node_indices array to receive node indices (caller allocated).
 @param max_results maximum number of results to return.
 @param[out] count actual number of nodes found.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_find_nodes_nearby(ENGEO_Handle gh, double x, double y,
                                          double radius, int *node_indices,
                                          int max_results, int *count);

/**
 @brief Finds links within a distance of a point.
 @param gh a geo context handle.
 @param x the X coordinate of the search point.
 @param y the Y coordinate of the search point.
 @param radius the search radius (in CRS units).
 @param[out] link_indices array to receive link indices (caller allocated).
 @param max_results maximum number of results to return.
 @param[out] count actual number of links found.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_find_links_nearby(ENGEO_Handle gh, double x, double y,
                                          double radius, int *link_indices,
                                          int max_results, int *count);

/**
 @brief Finds the nearest node to a point.
 @param gh a geo context handle.
 @param x the X coordinate.
 @param y the Y coordinate.
 @param[out] node_index the index of the nearest node.
 @param[out] distance the distance to the nearest node.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_find_nearest_node(ENGEO_Handle gh, double x, double y,
                                          int *node_index, double *distance);

/**
 @brief Finds the nearest link to a point.
 @param gh a geo context handle.
 @param x the X coordinate.
 @param y the Y coordinate.
 @param[out] link_index the index of the nearest link.
 @param[out] distance the distance to the nearest link.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_find_nearest_link(ENGEO_Handle gh, double x, double y,
                                          int *link_index, double *distance);

/**
 @brief Gets the bounding extent of the network.
 @param gh a geo context handle.
 @param[out] extent the network's bounding box.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_get_extent(ENGEO_Handle gh, ENGEO_Extent *extent);


/*=============================================================================
    Spatial Join Functions
=============================================================================*/

/**
 @brief Assigns attributes from polygons to nodes by location.
 @param gh a geo context handle.
 @param filepath path to polygon shapefile or GeoPackage.
 @param attr_name name of the polygon attribute to assign.
 @param target_prop the node property to update (e.g., tag).
 @return an error code.
 
 For each node, finds the polygon that contains it and copies the
 specified attribute value to the node's property.
*/
int DLLEXPORT_GEO ENGEO_spatial_join_nodes(ENGEO_Handle gh, const char *filepath,
                                           const char *attr_name,
                                           const char *target_prop);


/*=============================================================================
    Utility Functions
=============================================================================*/

/**
 @brief Gets the EPANET-Geo library version.
 @param[out] major major version number.
 @param[out] minor minor version number.
 @param[out] patch patch version number.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_getversion(int *major, int *minor, int *patch);

/**
 @brief Gets a text description of an error code.
 @param errcode the error code.
 @param[out] errmsg buffer to receive the error message.
 @param max_len maximum length of the buffer.
 @return an error code.
*/
int DLLEXPORT_GEO ENGEO_geterror(int errcode, char *errmsg, int max_len);

/**
 @brief Calculates the geodesic distance between two points.
 @param gh a geo context handle.
 @param x1 X coordinate of first point.
 @param y1 Y coordinate of first point.
 @param x2 X coordinate of second point.
 @param y2 Y coordinate of second point.
 @param[out] distance the calculated distance.
 @return an error code.
 
 If the CRS is geographic (lat/lon), returns geodesic distance in meters.
 Otherwise, returns Euclidean distance in CRS units.
*/
int DLLEXPORT_GEO ENGEO_calc_distance(ENGEO_Handle gh, double x1, double y1,
                                      double x2, double y2, double *distance);

/**
 @brief Calculates network pipe lengths from coordinates.
 @param gh a geo context handle.
 @param update_lengths whether to update pipe Length properties.
 @return an error code.
 
 Calculates the true length of each pipe based on node coordinates and
 vertices. If update_lengths is true, updates the EPANET link Length
 property; otherwise, just calculates without modifying the network.
*/
int DLLEXPORT_GEO ENGEO_calc_pipe_lengths(ENGEO_Handle gh, int update_lengths);

#if defined(__cplusplus)
}
#endif

#endif /* EPANET_GEO_H */
