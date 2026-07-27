/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_context.c
 Description:  geo context management functions
 Authors:      see AUTHORS
 Copyright:    see AUTHORS
 License:      see LICENSE
 Last Updated: 07/26/2026
 ******************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "geo_types.h"
#include "epanet_geo.h"

// GDAL/OGR headers
#include <gdal.h>
#include <cpl_conv.h>

// PROJ header
#include <proj.h>

// Library version
#define ENGEO_VERSION_MAJOR 1
#define ENGEO_VERSION_MINOR 0
#define ENGEO_VERSION_PATCH 0

// Static initialization flag
static int g_gdal_initialized = 0;

/*----------------------------------------------------------------
**  Initialize GDAL/OGR (called once)
**----------------------------------------------------------------*/
static int init_gdal(void)
{
    if (!g_gdal_initialized)
    {
        GDALAllRegister();
        g_gdal_initialized = 1;
    }
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Set error state
**----------------------------------------------------------------*/
void geo_set_error(ENGEO_Handle gh, int code, const char *msg)
{
    if (gh == NULL) return;
    gh->last_error_code = code;
    if (msg)
    {
        strncpy(gh->last_error, msg, GEO_MAX_ERRMSG - 1);
        gh->last_error[GEO_MAX_ERRMSG - 1] = '\0';
    }
    else
    {
        gh->last_error[0] = '\0';
    }
}

/*----------------------------------------------------------------
**  Clear error state
**----------------------------------------------------------------*/
void geo_clear_error(ENGEO_Handle gh)
{
    if (gh == NULL) return;
    gh->last_error_code = ENGEO_OK;
    gh->last_error[0] = '\0';
}

/*----------------------------------------------------------------
**  ENGEO_create - Create a new geo context
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_create(ENGEO_Handle *gh)
{
    struct GeoContext *ctx;
    int err;
    
    if (gh == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Initialize GDAL if needed
    err = init_gdal();
    if (err != ENGEO_OK) return err;
    
    // Allocate context
    ctx = (struct GeoContext *)calloc(1, sizeof(struct GeoContext));
    if (ctx == NULL) return ENGEO_ERR_OUT_OF_MEMORY;
    
    // Initialize PROJ context
    ctx->proj_context = proj_context_create();
    if (ctx->proj_context == NULL)
    {
        free(ctx);
        return ENGEO_ERR_PROJ_INIT;
    }
    
    // Initialize CRS context
    crs_init(&ctx->crs);
    
    // Initialize spatial index
    spatial_index_init(&ctx->spatial_index);
    
    // Mark as initialized
    ctx->gdal_initialized = 1;
    ctx->attached = 0;
    ctx->project = NULL;
    ctx->dem = NULL;
    
    geo_clear_error(ctx);
    
    *gh = ctx;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_destroy - Destroy a geo context
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_destroy(ENGEO_Handle *gh)
{
    struct GeoContext *ctx;
    
    if (gh == NULL || *gh == NULL) return ENGEO_ERR_INVALID_ARG;
    
    ctx = *gh;
    
    // Close DEM if open
    if (ctx->dem != NULL)
    {
        dem_close(ctx->dem);
        ctx->dem = NULL;
    }
    
    // Free CRS resources
    crs_free(&ctx->crs);
    
    // Free spatial index
    spatial_index_free(&ctx->spatial_index);
    
    // Destroy PROJ context
    if (ctx->proj_context != NULL)
    {
        proj_context_destroy(ctx->proj_context);
        ctx->proj_context = NULL;
    }
    
    // Free the context
    free(ctx);
    *gh = NULL;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_attach - Attach to an EPANET project
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_attach(ENGEO_Handle gh, EN_Project ph)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (ph == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Detach from any existing project
    if (gh->attached)
    {
        ENGEO_detach(gh);
    }
    
    gh->project = ph;
    gh->attached = 1;
    
    // Clear spatial index (needs rebuilding for new project)
    spatial_index_free(&gh->spatial_index);
    spatial_index_init(&gh->spatial_index);
    
    geo_clear_error(gh);
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_detach - Detach from EPANET project
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_detach(ENGEO_Handle gh)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    
    gh->project = NULL;
    gh->attached = 0;
    
    // Clear spatial index
    spatial_index_free(&gh->spatial_index);
    spatial_index_init(&gh->spatial_index);
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_getproject - Get attached project
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_getproject(ENGEO_Handle gh, EN_Project *ph)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (ph == NULL) return ENGEO_ERR_INVALID_ARG;
    
    *ph = gh->project;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_getversion - Get library version
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_getversion(int *major, int *minor, int *patch)
{
    if (major) *major = ENGEO_VERSION_MAJOR;
    if (minor) *minor = ENGEO_VERSION_MINOR;
    if (patch) *patch = ENGEO_VERSION_PATCH;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_geterror - Get error message
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_geterror(int errcode, char *errmsg, int max_len)
{
    const char *msg;
    
    if (errmsg == NULL || max_len < 1) return ENGEO_ERR_INVALID_ARG;
    
    switch (errcode)
    {
        case ENGEO_OK:
            msg = "No error";
            break;
        case ENGEO_ERR_NOT_INITIALIZED:
            msg = "Geo context not initialized";
            break;
        case ENGEO_ERR_NO_PROJECT:
            msg = "No EPANET project attached";
            break;
        case ENGEO_ERR_FILE_NOT_FOUND:
            msg = "File not found";
            break;
        case ENGEO_ERR_INVALID_FORMAT:
            msg = "Invalid or unsupported file format";
            break;
        case ENGEO_ERR_READ_FAILED:
            msg = "Failed to read file";
            break;
        case ENGEO_ERR_WRITE_FAILED:
            msg = "Failed to write file";
            break;
        case ENGEO_ERR_NO_CRS:
            msg = "No coordinate reference system defined";
            break;
        case ENGEO_ERR_CRS_MISMATCH:
            msg = "Coordinate reference system mismatch";
            break;
        case ENGEO_ERR_TRANSFORM_FAILED:
            msg = "Coordinate transformation failed";
            break;
        case ENGEO_ERR_NO_GEOMETRY:
            msg = "Feature has no geometry";
            break;
        case ENGEO_ERR_INVALID_GEOMETRY:
            msg = "Invalid geometry type for operation";
            break;
        case ENGEO_ERR_NO_DEM:
            msg = "No DEM file open";
            break;
        case ENGEO_ERR_OUT_OF_BOUNDS:
            msg = "Coordinates outside data extent";
            break;
        case ENGEO_ERR_GDAL_INIT:
            msg = "GDAL initialization failed";
            break;
        case ENGEO_ERR_PROJ_INIT:
            msg = "PROJ initialization failed";
            break;
        case ENGEO_ERR_GEOS_INIT:
            msg = "GEOS initialization failed";
            break;
        case ENGEO_ERR_OUT_OF_MEMORY:
            msg = "Memory allocation failed";
            break;
        case ENGEO_ERR_INVALID_ARG:
            msg = "Invalid argument";
            break;
        case ENGEO_ERR_NOT_SUPPORTED:
            msg = "Operation not supported";
            break;
        default:
            msg = "Unknown error";
            break;
    }
    
    strncpy(errmsg, msg, max_len - 1);
    errmsg[max_len - 1] = '\0';
    
    return ENGEO_OK;
}
