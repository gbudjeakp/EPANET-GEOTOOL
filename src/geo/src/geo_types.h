/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_types.h
 Description:  internal type definitions for epanet-geo
 Authors:      see AUTHORS
 Copyright:    see AUTHORS
 License:      see LICENSE
 Last Updated: 07/26/2026
 ******************************************************************************
*/

#ifndef GEO_TYPES_H
#define GEO_TYPES_H

#include "epanet_geo.h"
#include "epanet2_2.h"

// Include GDAL/OGR/PROJ headers
#include "gdal.h"
#include "ogr_srs_api.h"
#include "cpl_conv.h"
#include "proj.h"

// Maximum sizes
#define GEO_MAX_WKT      4096
#define GEO_MAX_PROJ_STR 1024
#define GEO_MAX_PATH     1024
#define GEO_MAX_ERRMSG   256

// Spatial index entry
typedef struct SpatialIndexEntry {
    int index;           // Node or link index
    int is_node;         // 1 for node, 0 for link
    double min_x;        // Bounding box
    double min_y;
    double max_x;
    double max_y;
} SpatialIndexEntry;

// Spatial index (simple array-based for now, can be upgraded to R-tree)
typedef struct SpatialIndex {
    SpatialIndexEntry *entries;
    int count;
    int capacity;
    int built;
} SpatialIndex;

// CRS context
typedef struct CRSContext {
    int type;                          // ENGEO_CRSType
    int epsg_code;                     // EPSG code if available
    char wkt[GEO_MAX_WKT];            // WKT string
    char proj_str[GEO_MAX_PROJ_STR];  // PROJ string
    OGRSpatialReferenceH srs;         // OGR spatial reference
    PJ *pj;                           // PROJ object
    int is_geographic;                 // 1 if lat/lon, 0 if projected
} CRSContext;

// DEM context
typedef struct DEMContext {
    GDALDatasetH dataset;             // GDAL raster dataset
    int width;                        // Raster width in pixels
    int height;                       // Raster height in pixels
    double geo_transform[6];          // Affine transform coefficients
    double no_data_value;             // NoData value
    int has_no_data;                  // Whether NoData is defined
    CRSContext crs;                   // DEM's CRS
    OGRCoordinateTransformationH transform_to_dem;  // Transform from network CRS to DEM CRS
} DEMContext;

// Main geo context structure
struct GeoContext {
    EN_Project project;               // Attached EPANET project
    int attached;                     // Whether a project is attached
    
    CRSContext crs;                   // Network CRS
    DEMContext *dem;                  // DEM context (NULL if no DEM open)
    SpatialIndex spatial_index;       // Spatial index for queries
    
    char last_error[GEO_MAX_ERRMSG];  // Last error message
    int last_error_code;              // Last error code
    
    // GDAL/PROJ initialization state
    int gdal_initialized;
    PJ_CONTEXT *proj_context;
};

// Internal utility functions
void geo_set_error(ENGEO_Handle gh, int code, const char *msg);
void geo_clear_error(ENGEO_Handle gh);

// CRS internal functions
int crs_init(CRSContext *ctx);
void crs_free(CRSContext *ctx);
int crs_from_epsg(CRSContext *ctx, int epsg, PJ_CONTEXT *pj_ctx);
int crs_from_wkt(CRSContext *ctx, const char *wkt, PJ_CONTEXT *pj_ctx);
int crs_from_proj(CRSContext *ctx, const char *proj_str, PJ_CONTEXT *pj_ctx);
int crs_to_wkt(CRSContext *ctx, char *wkt, int max_len);
int crs_create_transform(CRSContext *src, CRSContext *dst, 
                         OGRCoordinateTransformationH *transform,
                         PJ_CONTEXT *pj_ctx);

// Spatial index internal functions
int spatial_index_init(SpatialIndex *idx);
void spatial_index_free(SpatialIndex *idx);
int spatial_index_add(SpatialIndex *idx, int index, int is_node,
                      double min_x, double min_y, double max_x, double max_y);
int spatial_index_query_radius(SpatialIndex *idx, double x, double y, double radius,
                               int *results, int max_results, int *count,
                               int nodes_only, int links_only);

// DEM internal functions
int dem_open(DEMContext **dem, const char *filepath, PJ_CONTEXT *pj_ctx);
void dem_close(DEMContext *dem);
int dem_sample(DEMContext *dem, double x, double y, int method, double *value);
int dem_setup_transform(DEMContext *dem, CRSContext *network_crs, PJ_CONTEXT *pj_ctx);

// Import/export helpers
int import_detect_format(const char *filepath);
int import_shapefile_nodes(ENGEO_Handle gh, const char *filepath, int node_type,
                           const ENGEO_AttributeMap *attr_map);
int import_shapefile_links(ENGEO_Handle gh, const char *filepath,
                           const ENGEO_AttributeMap *attr_map);
int export_shapefile_nodes(ENGEO_Handle gh, const char *filepath, int node_type,
                           int include_results);
int export_shapefile_links(ENGEO_Handle gh, const char *filepath, int link_type,
                           int include_results);

// GeoJSON helpers
int import_geojson_layer(ENGEO_Handle gh, GDALDatasetH ds);
int export_geojson_network(ENGEO_Handle gh, const char *filepath, int include_results);

// GeoPackage helpers
int import_geopackage_tables(ENGEO_Handle gh, GDALDatasetH ds);
int export_geopackage_network(ENGEO_Handle gh, const char *filepath, int include_results);

// Geometry utilities
double geo_point_distance(double x1, double y1, double x2, double y2);
double geo_point_to_line_distance(double px, double py,
                                  double x1, double y1, double x2, double y2);
double geo_geodesic_distance(double lon1, double lat1, double lon2, double lat2);

#endif /* GEO_TYPES_H */
