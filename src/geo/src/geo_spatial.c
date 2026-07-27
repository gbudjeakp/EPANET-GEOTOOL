/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_spatial.c
 Description:  spatial query and index functions
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
#include <float.h>

#include "geo_types.h"
#include "epanet_geo.h"

/*----------------------------------------------------------------
** Initialize spatial index
**----------------------------------------------------------------*/
int spatial_index_init(SpatialIndex *idx)
{
    if (idx == NULL) return ENGEO_ERR_INVALID_ARG;
    
    idx->entries = NULL;
    idx->count = 0;
    idx->capacity = 0;
    idx->built = 0;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Free spatial index
**----------------------------------------------------------------*/
void spatial_index_free(SpatialIndex *idx)
{
    if (idx == NULL) return;
    
    if (idx->entries) {
        free(idx->entries);
        idx->entries = NULL;
    }
    
    idx->count = 0;
    idx->capacity = 0;
    idx->built = 0;
}

/*----------------------------------------------------------------
** Add entry to spatial index
**----------------------------------------------------------------*/
int spatial_index_add(SpatialIndex *idx, int index, int is_node,
                      double min_x, double min_y, double max_x, double max_y)
{
    if (idx == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Grow array if needed
    if (idx->count >= idx->capacity) {
        int new_capacity = idx->capacity == 0 ? 256 : idx->capacity * 2;
        SpatialIndexEntry *new_entries = (SpatialIndexEntry *)realloc(
            idx->entries, new_capacity * sizeof(SpatialIndexEntry));
        if (new_entries == NULL) {
            return ENGEO_ERR_OUT_OF_MEMORY;
        }
        idx->entries = new_entries;
        idx->capacity = new_capacity;
    }
    
    // Add entry
    SpatialIndexEntry *e = &idx->entries[idx->count];
    e->index = index;
    e->is_node = is_node;
    e->min_x = min_x;
    e->min_y = min_y;
    e->max_x = max_x;
    e->max_y = max_y;
    
    idx->count++;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Query spatial index by radius
**----------------------------------------------------------------*/
int spatial_index_query_radius(SpatialIndex *idx, double x, double y, double radius,
                               int *results, int max_results, int *count,
                               int nodes_only, int links_only)
{
    if (idx == NULL || results == NULL || count == NULL) {
        return ENGEO_ERR_INVALID_ARG;
    }
    
    *count = 0;
    
    double r2 = radius * radius;
    
    for (int i = 0; i < idx->count && *count < max_results; i++) {
        SpatialIndexEntry *e = &idx->entries[i];
        
        // Filter by type
        if (nodes_only && !e->is_node) continue;
        if (links_only && e->is_node) continue;
        
        // Quick bounding box check
        if (x + radius < e->min_x || x - radius > e->max_x ||
            y + radius < e->min_y || y - radius > e->max_y) {
            continue;
        }
        
        // For nodes (points), check actual distance
        if (e->is_node) {
            double cx = (e->min_x + e->max_x) / 2;
            double cy = (e->min_y + e->max_y) / 2;
            double d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d2 <= r2) {
                results[(*count)++] = e->index;
            }
        } else {
            // For links, the bounding box check is sufficient for now
            // Full implementation would check distance to line segment
            results[(*count)++] = e->index;
        }
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Build spatial index from network
**----------------------------------------------------------------*/
static int build_index_from_network(ENGEO_Handle gh)
{
    int nnodes, nlinks;
    int err;
    
    // Clear existing index
    spatial_index_free(&gh->spatial_index);
    spatial_index_init(&gh->spatial_index);
    
    // Add all nodes
    EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
    for (int i = 1; i <= nnodes; i++) {
        double x, y;
        err = EN_getcoord(gh->project, i, &x, &y);
        if (err == 0) {
            spatial_index_add(&gh->spatial_index, i, 1, x, y, x, y);
        }
    }
    
    // Add all links
    EN_getcount(gh->project, EN_LINKCOUNT, &nlinks);
    for (int i = 1; i <= nlinks; i++) {
        int n1, n2;
        double x1, y1, x2, y2;
        double min_x, min_y, max_x, max_y;
        
        EN_getlinknodes(gh->project, i, &n1, &n2);
        
        err = EN_getcoord(gh->project, n1, &x1, &y1);
        if (err != 0) continue;
        err = EN_getcoord(gh->project, n2, &x2, &y2);
        if (err != 0) continue;
        
        min_x = (x1 < x2) ? x1 : x2;
        min_y = (y1 < y2) ? y1 : y2;
        max_x = (x1 > x2) ? x1 : x2;
        max_y = (y1 > y2) ? y1 : y2;
        
        // Expand bounding box to include vertices
        int vertex_count;
        EN_getvertexcount(gh->project, i, &vertex_count);
        for (int v = 1; v <= vertex_count; v++) {
            double vx, vy;
            if (EN_getvertex(gh->project, i, v, &vx, &vy) == 0) {
                if (vx < min_x) min_x = vx;
                if (vx > max_x) max_x = vx;
                if (vy < min_y) min_y = vy;
                if (vy > max_y) max_y = vy;
            }
        }
        
        spatial_index_add(&gh->spatial_index, i, 0, min_x, min_y, max_x, max_y);
    }
    
    gh->spatial_index.built = 1;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_build_spatial_index
**  Builds a spatial index for fast queries
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_build_spatial_index(ENGEO_Handle gh)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    
    return build_index_from_network(gh);
}

/*----------------------------------------------------------------
**  ENGEO_clear_spatial_index
**  Clears the spatial index
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_clear_spatial_index(ENGEO_Handle gh)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    
    spatial_index_free(&gh->spatial_index);
    spatial_index_init(&gh->spatial_index);
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_find_nodes_nearby
**  Finds nodes within a distance of a point
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_find_nodes_nearby(ENGEO_Handle gh, double x, double y,
                                          double radius, int *node_indices,
                                          int max_results, int *count)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (node_indices == NULL || count == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Build index if not already built
    if (!gh->spatial_index.built) {
        int err = build_index_from_network(gh);
        if (err != ENGEO_OK) return err;
    }
    
    return spatial_index_query_radius(&gh->spatial_index, x, y, radius,
                                      node_indices, max_results, count, 1, 0);
}

/*----------------------------------------------------------------
**  ENGEO_find_links_nearby
**  Finds links within a distance of a point
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_find_links_nearby(ENGEO_Handle gh, double x, double y,
                                          double radius, int *link_indices,
                                          int max_results, int *count)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (link_indices == NULL || count == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Build index if not already built
    if (!gh->spatial_index.built) {
        int err = build_index_from_network(gh);
        if (err != ENGEO_OK) return err;
    }
    
    return spatial_index_query_radius(&gh->spatial_index, x, y, radius,
                                      link_indices, max_results, count, 0, 1);
}

/*----------------------------------------------------------------
**  ENGEO_find_nearest_node
**  Finds the nearest node to a point
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_find_nearest_node(ENGEO_Handle gh, double x, double y,
                                          int *node_index, double *distance)
{
    int nnodes;
    double min_dist = DBL_MAX;
    int nearest = 0;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (node_index == NULL || distance == NULL) return ENGEO_ERR_INVALID_ARG;
    
    EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
    
    for (int i = 1; i <= nnodes; i++) {
        double nx, ny;
        if (EN_getcoord(gh->project, i, &nx, &ny) == 0) {
            double d = sqrt((x - nx) * (x - nx) + (y - ny) * (y - ny));
            if (d < min_dist) {
                min_dist = d;
                nearest = i;
            }
        }
    }
    
    if (nearest == 0) {
        return ENGEO_ERR_NO_GEOMETRY;
    }
    
    *node_index = nearest;
    *distance = min_dist;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_find_nearest_link
**  Finds the nearest link to a point
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_find_nearest_link(ENGEO_Handle gh, double x, double y,
                                          int *link_index, double *distance)
{
    int nlinks;
    double min_dist = DBL_MAX;
    int nearest = 0;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (link_index == NULL || distance == NULL) return ENGEO_ERR_INVALID_ARG;
    
    EN_getcount(gh->project, EN_LINKCOUNT, &nlinks);
    
    for (int i = 1; i <= nlinks; i++) {
        int n1, n2;
        double x1, y1, x2, y2;
        
        EN_getlinknodes(gh->project, i, &n1, &n2);
        
        if (EN_getcoord(gh->project, n1, &x1, &y1) != 0) continue;
        if (EN_getcoord(gh->project, n2, &x2, &y2) != 0) continue;
        
        // Calculate distance to line segment
        double d = geo_point_to_line_distance(x, y, x1, y1, x2, y2);
        
        if (d < min_dist) {
            min_dist = d;
            nearest = i;
        }
    }
    
    if (nearest == 0) {
        return ENGEO_ERR_NO_GEOMETRY;
    }
    
    *link_index = nearest;
    *distance = min_dist;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_get_extent
**  Gets the bounding extent of the network
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_get_extent(ENGEO_Handle gh, ENGEO_Extent *extent)
{
    int nnodes, nlinks;
    double min_x = DBL_MAX, min_y = DBL_MAX;
    double max_x = -DBL_MAX, max_y = -DBL_MAX;
    int has_coords = 0;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (extent == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Check all nodes
    EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
    for (int i = 1; i <= nnodes; i++) {
        double x, y;
        if (EN_getcoord(gh->project, i, &x, &y) == 0) {
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
            has_coords = 1;
        }
    }
    
    // Check all link vertices
    EN_getcount(gh->project, EN_LINKCOUNT, &nlinks);
    for (int i = 1; i <= nlinks; i++) {
        int vertex_count;
        EN_getvertexcount(gh->project, i, &vertex_count);
        for (int v = 1; v <= vertex_count; v++) {
            double x, y;
            if (EN_getvertex(gh->project, i, v, &x, &y) == 0) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    
    if (!has_coords) {
        return ENGEO_ERR_NO_GEOMETRY;
    }
    
    extent->min_x = min_x;
    extent->min_y = min_y;
    extent->max_x = max_x;
    extent->max_y = max_y;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_spatial_join_nodes
**  Assigns attributes from polygons to nodes by location
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_spatial_join_nodes(ENGEO_Handle gh, const char *filepath,
                                           const char *attr_name,
                                           const char *target_prop)
{
    // This is a placeholder for spatial join functionality
    // Full implementation would:
    // 1. Open the polygon layer
    // 2. For each node, find containing polygon
    // 3. Copy attribute value to node property (likely as a tag)
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL || attr_name == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // TODO: Implement spatial join
    geo_set_error(gh, ENGEO_ERR_NOT_SUPPORTED, "Spatial join not yet implemented");
    return ENGEO_ERR_NOT_SUPPORTED;
}
