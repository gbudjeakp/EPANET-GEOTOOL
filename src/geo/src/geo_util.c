/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_util.c
 Description:  utility functions for geospatial operations
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

// Earth radius in meters (WGS84 mean)
#define EARTH_RADIUS_M 6371008.8

// Degrees to radians
#define DEG2RAD(d) ((d) * M_PI / 180.0)

/*----------------------------------------------------------------
** Calculate Euclidean distance between two points
**----------------------------------------------------------------*/
double geo_point_distance(double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

/*----------------------------------------------------------------
** Calculate distance from point to line segment
**----------------------------------------------------------------*/
double geo_point_to_line_distance(double px, double py,
                                  double x1, double y1, double x2, double y2)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    double len_sq = dx * dx + dy * dy;
    
    if (len_sq == 0) {
        // Degenerate line (start == end)
        return geo_point_distance(px, py, x1, y1);
    }
    
    // Project point onto line, clamped to segment
    double t = ((px - x1) * dx + (py - y1) * dy) / len_sq;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    
    double proj_x = x1 + t * dx;
    double proj_y = y1 + t * dy;
    
    return geo_point_distance(px, py, proj_x, proj_y);
}

/*----------------------------------------------------------------
** Calculate geodesic distance using Haversine formula
** Input: longitude/latitude in degrees
** Output: distance in meters
**----------------------------------------------------------------*/
double geo_geodesic_distance(double lon1, double lat1, double lon2, double lat2)
{
    double phi1 = DEG2RAD(lat1);
    double phi2 = DEG2RAD(lat2);
    double dphi = DEG2RAD(lat2 - lat1);
    double dlambda = DEG2RAD(lon2 - lon1);
    
    double a = sin(dphi / 2) * sin(dphi / 2) +
               cos(phi1) * cos(phi2) * sin(dlambda / 2) * sin(dlambda / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return EARTH_RADIUS_M * c;
}

/*----------------------------------------------------------------
**  ENGEO_calc_distance
**  Calculates the distance between two points
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_calc_distance(ENGEO_Handle gh, double x1, double y1,
                                      double x2, double y2, double *distance)
{
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (distance == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Use geodesic distance if CRS is geographic
    if (gh->crs.is_geographic) {
        *distance = geo_geodesic_distance(x1, y1, x2, y2);
    } else {
        *distance = geo_point_distance(x1, y1, x2, y2);
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_calc_pipe_lengths
**  Calculates network pipe lengths from coordinates
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_calc_pipe_lengths(ENGEO_Handle gh, int update_lengths)
{
    int nlinks;
    int err;
    int updated = 0;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    
    EN_getcount(gh->project, EN_LINKCOUNT, &nlinks);
    
    for (int i = 1; i <= nlinks; i++) {
        int link_type;
        EN_getlinktype(gh->project, i, &link_type);
        
        // Only calculate length for pipes
        if (link_type != EN_PIPE && link_type != EN_CVPIPE) continue;
        
        int n1, n2;
        double x1, y1, x2, y2;
        
        EN_getlinknodes(gh->project, i, &n1, &n2);
        
        err = EN_getcoord(gh->project, n1, &x1, &y1);
        if (err != 0) continue;
        err = EN_getcoord(gh->project, n2, &x2, &y2);
        if (err != 0) continue;
        
        // Calculate length including vertices
        double total_length = 0;
        double prev_x = x1, prev_y = y1;
        
        int vertex_count;
        EN_getvertexcount(gh->project, i, &vertex_count);
        
        for (int v = 1; v <= vertex_count; v++) {
            double vx, vy;
            if (EN_getvertex(gh->project, i, v, &vx, &vy) == 0) {
                if (gh->crs.is_geographic) {
                    total_length += geo_geodesic_distance(prev_x, prev_y, vx, vy);
                } else {
                    total_length += geo_point_distance(prev_x, prev_y, vx, vy);
                }
                prev_x = vx;
                prev_y = vy;
            }
        }
        
        // Add final segment
        if (gh->crs.is_geographic) {
            total_length += geo_geodesic_distance(prev_x, prev_y, x2, y2);
        } else {
            total_length += geo_point_distance(prev_x, prev_y, x2, y2);
        }
        
        // Update length if requested
        if (update_lengths && total_length > 0) {
            EN_setlinkvalue(gh->project, i, EN_LENGTH, total_length);
            updated++;
        }
    }
    
    if (updated == 0 && update_lengths) {
        geo_set_error(gh, ENGEO_ERR_NO_GEOMETRY, "No pipes with coordinates found");
        return ENGEO_ERR_NO_GEOMETRY;
    }
    
    return ENGEO_OK;
}
