/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_terrain.c
 Description:  DEM/terrain integration functions
 Authors:      see AUTHORS
 Copyright:    see AUTHORS
 License:      see LICENSE
 Last Updated: 07/26/2026
 ******************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "geo_types.h"
#include "epanet_geo.h"

// GDAL includes
#include "gdal.h"
#include "cpl_conv.h"
#include "ogr_srs_api.h"

/*----------------------------------------------------------------
** Open DEM file
**----------------------------------------------------------------*/
int dem_open(DEMContext **dem_ptr, const char *filepath, PJ_CONTEXT *pj_ctx)
{
    DEMContext *dem;
    GDALDatasetH ds;
    GDALRasterBandH band;
    
    if (dem_ptr == NULL || filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Open raster
    ds = GDALOpen(filepath, GA_ReadOnly);
    if (ds == NULL) {
        return ENGEO_ERR_FILE_NOT_FOUND;
    }
    
    // Check that it's a raster
    if (GDALGetRasterCount(ds) < 1) {
        GDALClose(ds);
        return ENGEO_ERR_NO_DEM;
    }
    
    // Allocate DEM context
    dem = (DEMContext *)calloc(1, sizeof(DEMContext));
    if (dem == NULL) {
        GDALClose(ds);
        return ENGEO_ERR_OUT_OF_MEMORY;
    }
    
    dem->dataset = ds;
    dem->width = GDALGetRasterXSize(ds);
    dem->height = GDALGetRasterYSize(ds);
    
    // Get geotransform
    if (GDALGetGeoTransform(ds, dem->geo_transform) != CE_None) {
        // No geotransform - use identity
        dem->geo_transform[0] = 0;    // top left x
        dem->geo_transform[1] = 1;    // pixel width
        dem->geo_transform[2] = 0;    // rotation
        dem->geo_transform[3] = dem->height;  // top left y
        dem->geo_transform[4] = 0;    // rotation
        dem->geo_transform[5] = -1;   // pixel height (negative)
    }
    
    // Get NoData value
    band = GDALGetRasterBand(ds, 1);
    int has_nodata;
    dem->no_data_value = GDALGetRasterNoDataValue(band, &has_nodata);
    dem->has_no_data = has_nodata;
    
    // Get CRS
    crs_init(&dem->crs);
    const char *proj_wkt = GDALGetProjectionRef(ds);
    if (proj_wkt && proj_wkt[0] != '\0') {
        crs_from_wkt(&dem->crs, proj_wkt, pj_ctx);
    }
    
    dem->transform_to_dem = NULL;
    
    *dem_ptr = dem;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Close DEM
**----------------------------------------------------------------*/
void dem_close(DEMContext *dem)
{
    if (dem == NULL) return;
    
    if (dem->dataset) {
        GDALClose(dem->dataset);
        dem->dataset = NULL;
    }
    
    if (dem->transform_to_dem) {
        OCTDestroyCoordinateTransformation(dem->transform_to_dem);
        dem->transform_to_dem = NULL;
    }
    
    crs_free(&dem->crs);
    
    free(dem);
}

/*----------------------------------------------------------------
** Set up transform from network CRS to DEM CRS
**----------------------------------------------------------------*/
int dem_setup_transform(DEMContext *dem, CRSContext *network_crs, PJ_CONTEXT *pj_ctx)
{
    if (dem == NULL || network_crs == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Clear existing transform
    if (dem->transform_to_dem) {
        OCTDestroyCoordinateTransformation(dem->transform_to_dem);
        dem->transform_to_dem = NULL;
    }
    
    // If DEM has no CRS or network has no CRS, assume they match
    if (dem->crs.type == ENGEO_CRS_NONE || network_crs->type == ENGEO_CRS_NONE) {
        return ENGEO_OK;
    }
    
    // Create transformation
    return crs_create_transform(network_crs, &dem->crs, &dem->transform_to_dem, pj_ctx);
}

/*----------------------------------------------------------------
** Sample DEM at a point
**----------------------------------------------------------------*/
int dem_sample(DEMContext *dem, double x, double y, int method, double *value)
{
    double px, py;
    int col, row;
    GDALRasterBandH band;
    float pixel_value;
    
    if (dem == NULL || value == NULL) return ENGEO_ERR_INVALID_ARG;
    if (dem->dataset == NULL) return ENGEO_ERR_NO_DEM;
    
    // Transform coordinates if needed
    double tx = x, ty = y;
    if (dem->transform_to_dem) {
        if (!OCTTransform(dem->transform_to_dem, 1, &tx, &ty, NULL)) {
            return ENGEO_ERR_TRANSFORM_FAILED;
        }
    }
    
    // Convert geo coordinates to pixel coordinates
    // Inverse geotransform: pixel = (geo - origin) / cell_size
    double det = dem->geo_transform[1] * dem->geo_transform[5] - 
                 dem->geo_transform[2] * dem->geo_transform[4];
    if (fabs(det) < 1e-10) {
        return ENGEO_ERR_INVALID_GEOMETRY;
    }
    
    px = (dem->geo_transform[5] * (tx - dem->geo_transform[0]) -
          dem->geo_transform[2] * (ty - dem->geo_transform[3])) / det;
    py = (-dem->geo_transform[4] * (tx - dem->geo_transform[0]) +
           dem->geo_transform[1] * (ty - dem->geo_transform[3])) / det;
    
    // Check bounds
    if (px < 0 || px >= dem->width || py < 0 || py >= dem->height) {
        return ENGEO_ERR_OUT_OF_BOUNDS;
    }
    
    band = GDALGetRasterBand(dem->dataset, 1);
    
    switch (method) {
        case ENGEO_ELEV_NEAREST:
        default:
            col = (int)(px + 0.5);
            row = (int)(py + 0.5);
            if (col >= dem->width) col = dem->width - 1;
            if (row >= dem->height) row = dem->height - 1;
            
            if (GDALRasterIO(band, GF_Read, col, row, 1, 1,
                            &pixel_value, 1, 1, GDT_Float32, 0, 0) != CE_None) {
                return ENGEO_ERR_READ_FAILED;
            }
            break;
            
        case ENGEO_ELEV_BILINEAR:
        {
            // Bilinear interpolation
            int x0 = (int)px;
            int y0 = (int)py;
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            
            if (x1 >= dem->width) x1 = x0;
            if (y1 >= dem->height) y1 = y0;
            
            float q11, q12, q21, q22;
            
            GDALRasterIO(band, GF_Read, x0, y0, 1, 1, &q11, 1, 1, GDT_Float32, 0, 0);
            GDALRasterIO(band, GF_Read, x1, y0, 1, 1, &q21, 1, 1, GDT_Float32, 0, 0);
            GDALRasterIO(band, GF_Read, x0, y1, 1, 1, &q12, 1, 1, GDT_Float32, 0, 0);
            GDALRasterIO(band, GF_Read, x1, y1, 1, 1, &q22, 1, 1, GDT_Float32, 0, 0);
            
            double fx = px - x0;
            double fy = py - y0;
            
            pixel_value = (float)(
                q11 * (1 - fx) * (1 - fy) +
                q21 * fx * (1 - fy) +
                q12 * (1 - fx) * fy +
                q22 * fx * fy
            );
        }
        break;
    }
    
    // Check for NoData
    if (dem->has_no_data && pixel_value == (float)dem->no_data_value) {
        return ENGEO_ERR_OUT_OF_BOUNDS;
    }
    
    *value = (double)pixel_value;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_open_dem
**  Opens a DEM raster file for elevation queries
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_open_dem(ENGEO_Handle gh, const char *filepath)
{
    int err;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Close existing DEM if open
    if (gh->dem != NULL) {
        dem_close(gh->dem);
        gh->dem = NULL;
    }
    
    // Open new DEM
    err = dem_open(&gh->dem, filepath, gh->proj_context);
    if (err != ENGEO_OK) {
        geo_set_error(gh, err, "Could not open DEM file");
        return err;
    }
    
    // Set up transform from network CRS to DEM CRS
    if (gh->crs.type != ENGEO_CRS_NONE) {
        err = dem_setup_transform(gh->dem, &gh->crs, gh->proj_context);
        if (err != ENGEO_OK) {
            // Non-fatal - assume CRS match
        }
    }
    
    geo_clear_error(gh);
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_close_dem
**  Closes the currently open DEM file
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_close_dem(ENGEO_Handle gh)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    
    if (gh->dem != NULL) {
        dem_close(gh->dem);
        gh->dem = NULL;
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_assign_elevations
**  Assigns elevations to all network nodes from the DEM
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_assign_elevations(ENGEO_Handle gh, int method)
{
    int i, nnodes;
    int err;
    int success_count = 0;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (gh->dem == NULL) return ENGEO_ERR_NO_DEM;
    
    EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
    
    for (i = 1; i <= nnodes; i++) {
        double x, y, elev;
        
        err = EN_getcoord(gh->project, i, &x, &y);
        if (err != 0) continue;  // No coordinates
        
        err = dem_sample(gh->dem, x, y, method, &elev);
        if (err == ENGEO_OK) {
            EN_setnodevalue(gh->project, i, EN_ELEVATION, elev);
            success_count++;
        }
    }
    
    if (success_count == 0) {
        geo_set_error(gh, ENGEO_ERR_OUT_OF_BOUNDS, "No nodes within DEM extent");
        return ENGEO_ERR_OUT_OF_BOUNDS;
    }
    
    geo_clear_error(gh);
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_assign_node_elevation
**  Assigns elevation to a single node from the DEM
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_assign_node_elevation(ENGEO_Handle gh, int node_index,
                                              int method)
{
    double x, y, elev;
    int err;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (gh->dem == NULL) return ENGEO_ERR_NO_DEM;
    
    err = EN_getcoord(gh->project, node_index, &x, &y);
    if (err != 0) {
        geo_set_error(gh, ENGEO_ERR_NO_GEOMETRY, "Node has no coordinates");
        return ENGEO_ERR_NO_GEOMETRY;
    }
    
    err = dem_sample(gh->dem, x, y, method, &elev);
    if (err != ENGEO_OK) {
        geo_set_error(gh, err, "Could not sample elevation");
        return err;
    }
    
    EN_setnodevalue(gh->project, node_index, EN_ELEVATION, elev);
    
    geo_clear_error(gh);
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_sample_elevation
**  Samples DEM elevation at a coordinate
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_sample_elevation(ENGEO_Handle gh, double x, double y,
                                         int method, double *elevation)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (elevation == NULL) return ENGEO_ERR_INVALID_ARG;
    if (gh->dem == NULL) return ENGEO_ERR_NO_DEM;
    
    return dem_sample(gh->dem, x, y, method, elevation);
}

/*----------------------------------------------------------------
**  ENGEO_get_pipe_profile
**  Gets elevation profile along a pipe
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_get_pipe_profile(ENGEO_Handle gh, int link_index,
                                         int num_samples, double *distances,
                                         double *elevations)
{
    int n1, n2;
    double x1, y1, x2, y2;
    int vertex_count;
    int err;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (gh->dem == NULL) return ENGEO_ERR_NO_DEM;
    if (distances == NULL || elevations == NULL) return ENGEO_ERR_INVALID_ARG;
    if (num_samples < 2) return ENGEO_ERR_INVALID_ARG;
    
    // Get end nodes
    EN_getlinknodes(gh->project, link_index, &n1, &n2);
    
    // Get coordinates
    err = EN_getcoord(gh->project, n1, &x1, &y1);
    if (err != 0) return ENGEO_ERR_NO_GEOMETRY;
    err = EN_getcoord(gh->project, n2, &x2, &y2);
    if (err != 0) return ENGEO_ERR_NO_GEOMETRY;
    
    // Get vertices
    EN_getvertexcount(gh->project, link_index, &vertex_count);
    
    // Build array of all points along the pipe
    int total_points = 2 + vertex_count;
    double *px = (double *)malloc(total_points * sizeof(double));
    double *py = (double *)malloc(total_points * sizeof(double));
    double *cum_dist = (double *)malloc(total_points * sizeof(double));
    
    if (px == NULL || py == NULL || cum_dist == NULL) {
        free(px);
        free(py);
        free(cum_dist);
        return ENGEO_ERR_OUT_OF_MEMORY;
    }
    
    // Start point
    px[0] = x1;
    py[0] = y1;
    cum_dist[0] = 0;
    
    // Vertices
    double total_dist = 0;
    double prev_x = x1, prev_y = y1;
    
    for (int i = 0; i < vertex_count; i++) {
        double vx, vy;
        EN_getvertex(gh->project, link_index, i + 1, &vx, &vy);
        px[i + 1] = vx;
        py[i + 1] = vy;
        total_dist += sqrt((vx - prev_x) * (vx - prev_x) + (vy - prev_y) * (vy - prev_y));
        cum_dist[i + 1] = total_dist;
        prev_x = vx;
        prev_y = vy;
    }
    
    // End point
    px[total_points - 1] = x2;
    py[total_points - 1] = y2;
    total_dist += sqrt((x2 - prev_x) * (x2 - prev_x) + (y2 - prev_y) * (y2 - prev_y));
    cum_dist[total_points - 1] = total_dist;
    
    // Sample at evenly spaced intervals
    for (int i = 0; i < num_samples; i++) {
        double target_dist = (i * total_dist) / (num_samples - 1);
        distances[i] = target_dist;
        
        // Find segment containing this distance
        int seg = 0;
        for (int j = 1; j < total_points; j++) {
            if (cum_dist[j] >= target_dist) {
                seg = j - 1;
                break;
            }
        }
        
        // Interpolate position within segment
        double seg_start = cum_dist[seg];
        double seg_end = cum_dist[seg + 1];
        double t = (seg_end > seg_start) ? (target_dist - seg_start) / (seg_end - seg_start) : 0;
        
        double sx = px[seg] + t * (px[seg + 1] - px[seg]);
        double sy = py[seg] + t * (py[seg + 1] - py[seg]);
        
        // Sample elevation
        err = dem_sample(gh->dem, sx, sy, ENGEO_ELEV_BILINEAR, &elevations[i]);
        if (err != ENGEO_OK) {
            elevations[i] = 0;  // Use 0 for out-of-bounds
        }
    }
    
    free(px);
    free(py);
    free(cum_dist);
    
    return ENGEO_OK;
}
