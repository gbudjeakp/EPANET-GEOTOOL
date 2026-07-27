/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_crs.c
 Description:  coordinate reference system functions
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
#include <ogr_srs_api.h>
#include <ogr_api.h>

// PROJ header
#include <proj.h>

/*----------------------------------------------------------------
**  Initialize CRS context
**----------------------------------------------------------------*/
int crs_init(CRSContext *ctx)
{
    if (ctx == NULL) return ENGEO_ERR_INVALID_ARG;
    
    memset(ctx, 0, sizeof(CRSContext));
    ctx->type = ENGEO_CRS_NONE;
    ctx->epsg_code = 0;
    ctx->srs = NULL;
    ctx->pj = NULL;
    ctx->is_geographic = 0;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Free CRS context resources
**----------------------------------------------------------------*/
void crs_free(CRSContext *ctx)
{
    if (ctx == NULL) return;
    
    if (ctx->srs != NULL)
    {
        OSRDestroySpatialReference(ctx->srs);
        ctx->srs = NULL;
    }
    
    if (ctx->pj != NULL)
    {
        proj_destroy(ctx->pj);
        ctx->pj = NULL;
    }
    
    ctx->type = ENGEO_CRS_NONE;
    ctx->epsg_code = 0;
    ctx->wkt[0] = '\0';
    ctx->proj_str[0] = '\0';
}

/*----------------------------------------------------------------
**  Create CRS from EPSG code
**----------------------------------------------------------------*/
int crs_from_epsg(CRSContext *ctx, int epsg, PJ_CONTEXT *pj_ctx)
{
    char epsg_str[32];
    
    if (ctx == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Free existing resources
    crs_free(ctx);
    
    // Create OGR spatial reference
    ctx->srs = OSRNewSpatialReference(NULL);
    if (ctx->srs == NULL) return ENGEO_ERR_OUT_OF_MEMORY;
    
    if (OSRImportFromEPSG(ctx->srs, epsg) != OGRERR_NONE)
    {
        OSRDestroySpatialReference(ctx->srs);
        ctx->srs = NULL;
        return ENGEO_ERR_INVALID_ARG;
    }
    
    // Create PROJ object
    snprintf(epsg_str, sizeof(epsg_str), "EPSG:%d", epsg);
    ctx->pj = proj_create(pj_ctx, epsg_str);
    if (ctx->pj == NULL)
    {
        OSRDestroySpatialReference(ctx->srs);
        ctx->srs = NULL;
        return ENGEO_ERR_PROJ_INIT;
    }
    
    // Store info
    ctx->type = ENGEO_CRS_EPSG;
    ctx->epsg_code = epsg;
    ctx->is_geographic = OSRIsGeographic(ctx->srs);
    
    // Export to WKT for storage
    char *wkt = NULL;
    if (OSRExportToWkt(ctx->srs, &wkt) == OGRERR_NONE && wkt != NULL)
    {
        strncpy(ctx->wkt, wkt, GEO_MAX_WKT - 1);
        ctx->wkt[GEO_MAX_WKT - 1] = '\0';
        CPLFree(wkt);
    }
    
    // Export to PROJ string
    strncpy(ctx->proj_str, epsg_str, GEO_MAX_PROJ_STR - 1);
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Create CRS from WKT string
**----------------------------------------------------------------*/
int crs_from_wkt(CRSContext *ctx, const char *wkt, PJ_CONTEXT *pj_ctx)
{
    if (ctx == NULL || wkt == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Free existing resources
    crs_free(ctx);
    
    // Create OGR spatial reference
    ctx->srs = OSRNewSpatialReference(NULL);
    if (ctx->srs == NULL) return ENGEO_ERR_OUT_OF_MEMORY;
    
    if (OSRImportFromWkt(ctx->srs, (char**)&wkt) != OGRERR_NONE)
    {
        OSRDestroySpatialReference(ctx->srs);
        ctx->srs = NULL;
        return ENGEO_ERR_INVALID_ARG;
    }
    
    // Create PROJ object from WKT
    ctx->pj = proj_create(pj_ctx, wkt);
    if (ctx->pj == NULL)
    {
        OSRDestroySpatialReference(ctx->srs);
        ctx->srs = NULL;
        return ENGEO_ERR_PROJ_INIT;
    }
    
    // Store info
    ctx->type = ENGEO_CRS_WKT;
    strncpy(ctx->wkt, wkt, GEO_MAX_WKT - 1);
    ctx->wkt[GEO_MAX_WKT - 1] = '\0';
    ctx->is_geographic = OSRIsGeographic(ctx->srs);
    
    // Try to get EPSG code
    const char *authority = OSRGetAuthorityName(ctx->srs, NULL);
    if (authority != NULL && strcmp(authority, "EPSG") == 0)
    {
        const char *code = OSRGetAuthorityCode(ctx->srs, NULL);
        if (code != NULL)
        {
            ctx->epsg_code = atoi(code);
        }
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Create CRS from PROJ string
**----------------------------------------------------------------*/
int crs_from_proj(CRSContext *ctx, const char *proj_str, PJ_CONTEXT *pj_ctx)
{
    if (ctx == NULL || proj_str == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Free existing resources
    crs_free(ctx);
    
    // Create PROJ object
    ctx->pj = proj_create(pj_ctx, proj_str);
    if (ctx->pj == NULL)
    {
        return ENGEO_ERR_PROJ_INIT;
    }
    
    // Create OGR spatial reference from PROJ
    ctx->srs = OSRNewSpatialReference(NULL);
    if (ctx->srs == NULL)
    {
        proj_destroy(ctx->pj);
        ctx->pj = NULL;
        return ENGEO_ERR_OUT_OF_MEMORY;
    }
    
    if (OSRImportFromProj4(ctx->srs, proj_str) != OGRERR_NONE)
    {
        // Try as WKT
        if (OSRSetFromUserInput(ctx->srs, proj_str) != OGRERR_NONE)
        {
            OSRDestroySpatialReference(ctx->srs);
            proj_destroy(ctx->pj);
            ctx->srs = NULL;
            ctx->pj = NULL;
            return ENGEO_ERR_INVALID_ARG;
        }
    }
    
    // Store info
    ctx->type = ENGEO_CRS_PROJ;
    strncpy(ctx->proj_str, proj_str, GEO_MAX_PROJ_STR - 1);
    ctx->proj_str[GEO_MAX_PROJ_STR - 1] = '\0';
    ctx->is_geographic = OSRIsGeographic(ctx->srs);
    
    // Export to WKT
    char *wkt = NULL;
    if (OSRExportToWkt(ctx->srs, &wkt) == OGRERR_NONE && wkt != NULL)
    {
        strncpy(ctx->wkt, wkt, GEO_MAX_WKT - 1);
        ctx->wkt[GEO_MAX_WKT - 1] = '\0';
        CPLFree(wkt);
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Export CRS to WKT
**----------------------------------------------------------------*/
int crs_to_wkt(CRSContext *ctx, char *wkt, int max_len)
{
    if (ctx == NULL || wkt == NULL || max_len < 1) return ENGEO_ERR_INVALID_ARG;
    if (ctx->type == ENGEO_CRS_NONE) return ENGEO_ERR_NO_CRS;
    
    strncpy(wkt, ctx->wkt, max_len - 1);
    wkt[max_len - 1] = '\0';
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Create coordinate transformation between CRS
**----------------------------------------------------------------*/
int crs_create_transform(CRSContext *src, CRSContext *dst,
                         OGRCoordinateTransformationH *transform,
                         PJ_CONTEXT *pj_ctx)
{
    (void)pj_ctx;  // May be used for PROJ-based transform in future
    
    if (src == NULL || dst == NULL || transform == NULL)
        return ENGEO_ERR_INVALID_ARG;
    
    if (src->type == ENGEO_CRS_NONE || dst->type == ENGEO_CRS_NONE)
        return ENGEO_ERR_NO_CRS;
    
    if (src->srs == NULL || dst->srs == NULL)
        return ENGEO_ERR_NO_CRS;
    
    *transform = OCTNewCoordinateTransformation(src->srs, dst->srs);
    if (*transform == NULL)
    {
        return ENGEO_ERR_TRANSFORM_FAILED;
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Public API: Set CRS from EPSG
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_setcrs_epsg(ENGEO_Handle gh, int epsg_code)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    
    int err = crs_from_epsg(&gh->crs, epsg_code, gh->proj_context);
    if (err != ENGEO_OK)
    {
        geo_set_error(gh, err, "Failed to set CRS from EPSG code");
    }
    return err;
}

/*----------------------------------------------------------------
**  Public API: Set CRS from WKT
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_setcrs_wkt(ENGEO_Handle gh, const char *wkt)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (wkt == NULL) return ENGEO_ERR_INVALID_ARG;
    
    int err = crs_from_wkt(&gh->crs, wkt, gh->proj_context);
    if (err != ENGEO_OK)
    {
        geo_set_error(gh, err, "Failed to set CRS from WKT");
    }
    return err;
}

/*----------------------------------------------------------------
**  Public API: Set CRS from PROJ string
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_setcrs_proj(ENGEO_Handle gh, const char *proj_str)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (proj_str == NULL) return ENGEO_ERR_INVALID_ARG;
    
    int err = crs_from_proj(&gh->crs, proj_str, gh->proj_context);
    if (err != ENGEO_OK)
    {
        geo_set_error(gh, err, "Failed to set CRS from PROJ string");
    }
    return err;
}

/*----------------------------------------------------------------
**  Public API: Get CRS type
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_getcrs_type(ENGEO_Handle gh, int *crs_type)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (crs_type == NULL) return ENGEO_ERR_INVALID_ARG;
    
    *crs_type = gh->crs.type;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Public API: Get CRS as EPSG code
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_getcrs_epsg(ENGEO_Handle gh, int *epsg_code)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (epsg_code == NULL) return ENGEO_ERR_INVALID_ARG;
    
    *epsg_code = gh->crs.epsg_code;
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Public API: Get CRS as WKT
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_getcrs_wkt(ENGEO_Handle gh, char *wkt, int max_len)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (wkt == NULL || max_len < 1) return ENGEO_ERR_INVALID_ARG;
    
    if (gh->crs.type == ENGEO_CRS_NONE)
    {
        wkt[0] = '\0';
        return ENGEO_ERR_NO_CRS;
    }
    
    return crs_to_wkt(&gh->crs, wkt, max_len);
}

/*----------------------------------------------------------------
**  Public API: Transform network coordinates
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_transform_network(ENGEO_Handle gh, int target_epsg)
{
    CRSContext target_crs;
    OGRCoordinateTransformationH transform = NULL;
    int err;
    int i, j;
    int nnodes, nlinks;
    double x, y;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached || gh->project == NULL) return ENGEO_ERR_NO_PROJECT;
    if (gh->crs.type == ENGEO_CRS_NONE) return ENGEO_ERR_NO_CRS;
    
    // Initialize target CRS
    crs_init(&target_crs);
    err = crs_from_epsg(&target_crs, target_epsg, gh->proj_context);
    if (err != ENGEO_OK)
    {
        geo_set_error(gh, err, "Invalid target EPSG code");
        return err;
    }
    
    // Create transformation
    err = crs_create_transform(&gh->crs, &target_crs, &transform, gh->proj_context);
    if (err != ENGEO_OK)
    {
        crs_free(&target_crs);
        geo_set_error(gh, err, "Failed to create coordinate transformation");
        return err;
    }
    
    // Get network size
    EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
    EN_getcount(gh->project, EN_LINKCOUNT, &nlinks);
    
    // Transform all node coordinates
    for (i = 1; i <= nnodes; i++)
    {
        err = EN_getcoord(gh->project, i, &x, &y);
        if (err == 0)  // Has coordinates
        {
            if (OCTTransform(transform, 1, &x, &y, NULL))
            {
                EN_setcoord(gh->project, i, x, y);
            }
        }
    }
    
    // Transform all link vertices
    for (i = 1; i <= nlinks; i++)
    {
        int vertex_count = 0;
        EN_getvertexcount(gh->project, i, &vertex_count);
        
        for (j = 1; j <= vertex_count; j++)
        {
            err = EN_getvertex(gh->project, i, j, &x, &y);
            if (err == 0)
            {
                if (OCTTransform(transform, 1, &x, &y, NULL))
                {
                    EN_setvertex(gh->project, i, j, x, y);
                }
            }
        }
    }
    
    // Destroy transformation
    OCTDestroyCoordinateTransformation(transform);
    
    // Update context CRS to target
    crs_free(&gh->crs);
    gh->crs = target_crs;
    
    // Invalidate spatial index
    spatial_index_free(&gh->spatial_index);
    spatial_index_init(&gh->spatial_index);
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  Public API: Transform a single point
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_transform_point(ENGEO_Handle gh, int target_epsg,
                                        double *x, double *y)
{
    CRSContext target_crs;
    OGRCoordinateTransformationH transform = NULL;
    int err;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (x == NULL || y == NULL) return ENGEO_ERR_INVALID_ARG;
    if (gh->crs.type == ENGEO_CRS_NONE) return ENGEO_ERR_NO_CRS;
    
    // Initialize target CRS
    crs_init(&target_crs);
    err = crs_from_epsg(&target_crs, target_epsg, gh->proj_context);
    if (err != ENGEO_OK)
    {
        return err;
    }
    
    // Create transformation
    err = crs_create_transform(&gh->crs, &target_crs, &transform, gh->proj_context);
    if (err != ENGEO_OK)
    {
        crs_free(&target_crs);
        return err;
    }
    
    // Transform the point
    if (!OCTTransform(transform, 1, x, y, NULL))
    {
        OCTDestroyCoordinateTransformation(transform);
        crs_free(&target_crs);
        return ENGEO_ERR_TRANSFORM_FAILED;
    }
    
    OCTDestroyCoordinateTransformation(transform);
    crs_free(&target_crs);
    
    return ENGEO_OK;
}
