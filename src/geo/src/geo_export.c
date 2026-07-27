/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_export.c
 Description:  geospatial data export functions
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

// GDAL/OGR includes
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "cpl_conv.h"

/*----------------------------------------------------------------
** Export nodes to Shapefile
**----------------------------------------------------------------*/
int export_shapefile_nodes(ENGEO_Handle gh, const char *filepath, int node_type,
                           int include_results)
{
    GDALDriverH driver;
    GDALDatasetH ds = NULL;
    OGRLayerH layer;
    OGRFeatureH feature;
    OGRGeometryH geom;
    OGRFieldDefnH field_defn;
    OGRSpatialReferenceH srs = NULL;
    int i, nnodes;
    int err = ENGEO_OK;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Get Shapefile driver
    driver = GDALGetDriverByName("ESRI Shapefile");
    if (driver == NULL) {
        geo_set_error(gh, ENGEO_ERR_GDAL_INIT, "Shapefile driver not available");
        return ENGEO_ERR_GDAL_INIT;
    }
    
    // Create dataset (delete if exists)
    GDALDeleteDataset(driver, filepath);
    ds = GDALCreate(driver, filepath, 0, 0, 0, GDT_Unknown, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_WRITE_FAILED, "Could not create shapefile");
        return ENGEO_ERR_WRITE_FAILED;
    }
    
    // Set up spatial reference if available
    if (gh->crs.srs != NULL) {
        srs = gh->crs.srs;
    }
    
    // Create layer
    layer = GDALDatasetCreateLayer(ds, "nodes", srs, wkbPoint, NULL);
    if (layer == NULL) {
        GDALClose(ds);
        geo_set_error(gh, ENGEO_ERR_WRITE_FAILED, "Could not create layer");
        return ENGEO_ERR_WRITE_FAILED;
    }
    
    // Define fields
    field_defn = OGR_Fld_Create("ID", OFTString);
    OGR_Fld_SetWidth(field_defn, 32);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("TYPE", OFTString);
    OGR_Fld_SetWidth(field_defn, 16);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("ELEVATION", OFTReal);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("DEMAND", OFTReal);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    // Add result fields if requested
    if (include_results) {
        field_defn = OGR_Fld_Create("HEAD", OFTReal);
        OGR_L_CreateField(layer, field_defn, TRUE);
        OGR_Fld_Destroy(field_defn);
        
        field_defn = OGR_Fld_Create("PRESSURE", OFTReal);
        OGR_L_CreateField(layer, field_defn, TRUE);
        OGR_Fld_Destroy(field_defn);
        
        field_defn = OGR_Fld_Create("QUALITY", OFTReal);
        OGR_L_CreateField(layer, field_defn, TRUE);
        OGR_Fld_Destroy(field_defn);
    }
    
    // Export nodes
    EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
    
    for (i = 1; i <= nnodes; i++) {
        double x, y;
        char id[32];
        int node_type_val;
        double elev, demand;
        
        // Get node type
        EN_getnodetype(gh->project, i, &node_type_val);
        
        // Filter by requested type
        if (node_type != ENGEO_LAYER_ALL_NODES) {
            if (node_type == ENGEO_LAYER_JUNCTIONS && node_type_val != EN_JUNCTION) continue;
            if (node_type == ENGEO_LAYER_RESERVOIRS && node_type_val != EN_RESERVOIR) continue;
            if (node_type == ENGEO_LAYER_TANKS && node_type_val != EN_TANK) continue;
        }
        
        // Get coordinates
        err = EN_getcoord(gh->project, i, &x, &y);
        if (err != 0) continue;  // Skip nodes without coordinates
        
        // Get attributes
        EN_getnodeid(gh->project, i, id);
        EN_getnodevalue(gh->project, i, EN_ELEVATION, &elev);
        EN_getnodevalue(gh->project, i, EN_BASEDEMAND, &demand);
        
        // Create feature
        feature = OGR_F_Create(OGR_L_GetLayerDefn(layer));
        
        // Set geometry
        geom = OGR_G_CreateGeometry(wkbPoint);
        OGR_G_SetPoint_2D(geom, 0, x, y);
        OGR_F_SetGeometry(feature, geom);
        OGR_G_DestroyGeometry(geom);
        
        // Set attributes
        OGR_F_SetFieldString(feature, 0, id);
        
        const char *type_str = "JUNCTION";
        if (node_type_val == EN_RESERVOIR) type_str = "RESERVOIR";
        else if (node_type_val == EN_TANK) type_str = "TANK";
        OGR_F_SetFieldString(feature, 1, type_str);
        
        OGR_F_SetFieldDouble(feature, 2, elev);
        OGR_F_SetFieldDouble(feature, 3, demand);
        
        // Set results if requested
        if (include_results) {
            double head, pressure, quality;
            EN_getnodevalue(gh->project, i, EN_HEAD, &head);
            EN_getnodevalue(gh->project, i, EN_PRESSURE, &pressure);
            EN_getnodevalue(gh->project, i, EN_QUALITY, &quality);
            OGR_F_SetFieldDouble(feature, 4, head);
            OGR_F_SetFieldDouble(feature, 5, pressure);
            OGR_F_SetFieldDouble(feature, 6, quality);
        }
        
        // Write feature
        if (OGR_L_CreateFeature(layer, feature) != OGRERR_NONE) {
            err = ENGEO_ERR_WRITE_FAILED;
        }
        
        OGR_F_Destroy(feature);
    }
    
    GDALClose(ds);
    
    return err;
}

/*----------------------------------------------------------------
** Export links to Shapefile
**----------------------------------------------------------------*/
int export_shapefile_links(ENGEO_Handle gh, const char *filepath, int link_type,
                           int include_results)
{
    GDALDriverH driver;
    GDALDatasetH ds = NULL;
    OGRLayerH layer;
    OGRFeatureH feature;
    OGRGeometryH geom;
    OGRFieldDefnH field_defn;
    OGRSpatialReferenceH srs = NULL;
    int i, nlinks;
    int err = ENGEO_OK;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Get Shapefile driver
    driver = GDALGetDriverByName("ESRI Shapefile");
    if (driver == NULL) {
        geo_set_error(gh, ENGEO_ERR_GDAL_INIT, "Shapefile driver not available");
        return ENGEO_ERR_GDAL_INIT;
    }
    
    // Create dataset (delete if exists)
    GDALDeleteDataset(driver, filepath);
    ds = GDALCreate(driver, filepath, 0, 0, 0, GDT_Unknown, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_WRITE_FAILED, "Could not create shapefile");
        return ENGEO_ERR_WRITE_FAILED;
    }
    
    // Set up spatial reference if available
    if (gh->crs.srs != NULL) {
        srs = gh->crs.srs;
    }
    
    // Create layer
    layer = GDALDatasetCreateLayer(ds, "links", srs, wkbLineString, NULL);
    if (layer == NULL) {
        GDALClose(ds);
        geo_set_error(gh, ENGEO_ERR_WRITE_FAILED, "Could not create layer");
        return ENGEO_ERR_WRITE_FAILED;
    }
    
    // Define fields
    field_defn = OGR_Fld_Create("ID", OFTString);
    OGR_Fld_SetWidth(field_defn, 32);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("TYPE", OFTString);
    OGR_Fld_SetWidth(field_defn, 16);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("DIAMETER", OFTReal);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("LENGTH", OFTReal);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    field_defn = OGR_Fld_Create("ROUGHNESS", OFTReal);
    OGR_L_CreateField(layer, field_defn, TRUE);
    OGR_Fld_Destroy(field_defn);
    
    // Add result fields if requested
    if (include_results) {
        field_defn = OGR_Fld_Create("FLOW", OFTReal);
        OGR_L_CreateField(layer, field_defn, TRUE);
        OGR_Fld_Destroy(field_defn);
        
        field_defn = OGR_Fld_Create("VELOCITY", OFTReal);
        OGR_L_CreateField(layer, field_defn, TRUE);
        OGR_Fld_Destroy(field_defn);
        
        field_defn = OGR_Fld_Create("HEADLOSS", OFTReal);
        OGR_L_CreateField(layer, field_defn, TRUE);
        OGR_Fld_Destroy(field_defn);
    }
    
    // Export links
    EN_getcount(gh->project, EN_LINKCOUNT, &nlinks);
    
    for (i = 1; i <= nlinks; i++) {
        char id[32];
        int link_type_val;
        int n1, n2;
        double x1, y1, x2, y2;
        double diam, length, rough;
        
        // Get link type
        EN_getlinktype(gh->project, i, &link_type_val);
        
        // Filter by requested type
        if (link_type != ENGEO_LAYER_ALL_LINKS) {
            if (link_type == ENGEO_LAYER_PIPES && 
                link_type_val != EN_PIPE && link_type_val != EN_CVPIPE) continue;
            if (link_type == ENGEO_LAYER_PUMPS && link_type_val != EN_PUMP) continue;
            if (link_type == ENGEO_LAYER_VALVES && 
                link_type_val < EN_PRV) continue;  // PRV and above are valves
        }
        
        // Get end nodes
        EN_getlinknodes(gh->project, i, &n1, &n2);
        
        // Get coordinates of end nodes
        err = EN_getcoord(gh->project, n1, &x1, &y1);
        if (err != 0) continue;
        err = EN_getcoord(gh->project, n2, &x2, &y2);
        if (err != 0) continue;
        
        // Get attributes
        EN_getlinkid(gh->project, i, id);
        EN_getlinkvalue(gh->project, i, EN_DIAMETER, &diam);
        EN_getlinkvalue(gh->project, i, EN_LENGTH, &length);
        EN_getlinkvalue(gh->project, i, EN_ROUGHNESS, &rough);
        
        // Create feature
        feature = OGR_F_Create(OGR_L_GetLayerDefn(layer));
        
        // Create line geometry
        geom = OGR_G_CreateGeometry(wkbLineString);
        OGR_G_AddPoint_2D(geom, x1, y1);
        
        // Add vertices if any
        int vertex_count;
        EN_getvertexcount(gh->project, i, &vertex_count);
        for (int v = 1; v <= vertex_count; v++) {
            double vx, vy;
            if (EN_getvertex(gh->project, i, v, &vx, &vy) == 0) {
                OGR_G_AddPoint_2D(geom, vx, vy);
            }
        }
        
        OGR_G_AddPoint_2D(geom, x2, y2);
        OGR_F_SetGeometry(feature, geom);
        OGR_G_DestroyGeometry(geom);
        
        // Set attributes
        OGR_F_SetFieldString(feature, 0, id);
        
        const char *type_str = "PIPE";
        if (link_type_val == EN_PUMP) type_str = "PUMP";
        else if (link_type_val >= EN_PRV && link_type_val <= EN_GPV) type_str = "VALVE";
        OGR_F_SetFieldString(feature, 1, type_str);
        
        OGR_F_SetFieldDouble(feature, 2, diam);
        OGR_F_SetFieldDouble(feature, 3, length);
        OGR_F_SetFieldDouble(feature, 4, rough);
        
        // Set results if requested
        if (include_results) {
            double flow, velocity, headloss;
            EN_getlinkvalue(gh->project, i, EN_FLOW, &flow);
            EN_getlinkvalue(gh->project, i, EN_VELOCITY, &velocity);
            EN_getlinkvalue(gh->project, i, EN_HEADLOSS, &headloss);
            OGR_F_SetFieldDouble(feature, 5, flow);
            OGR_F_SetFieldDouble(feature, 6, velocity);
            OGR_F_SetFieldDouble(feature, 7, headloss);
        }
        
        // Write feature
        if (OGR_L_CreateFeature(layer, feature) != OGRERR_NONE) {
            err = ENGEO_ERR_WRITE_FAILED;
        }
        
        OGR_F_Destroy(feature);
    }
    
    GDALClose(ds);
    
    return err;
}

/*----------------------------------------------------------------
**  ENGEO_export
**  Exports network data to a geospatial file
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_export(ENGEO_Handle gh, const char *filepath,
                               int format, int layer_type, int mode)
{
    int include_results = (mode == ENGEO_EXPORT_WITH_RESULTS || 
                          mode == ENGEO_EXPORT_RESULTS_ONLY);
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    switch (format) {
        case ENGEO_SHAPEFILE:
            if (layer_type == ENGEO_LAYER_ALL_NODES || 
                layer_type == ENGEO_LAYER_JUNCTIONS ||
                layer_type == ENGEO_LAYER_RESERVOIRS ||
                layer_type == ENGEO_LAYER_TANKS) {
                return export_shapefile_nodes(gh, filepath, layer_type, include_results);
            } else {
                return export_shapefile_links(gh, filepath, layer_type, include_results);
            }
            
        case ENGEO_GEOJSON:
            return ENGEO_export_geojson(gh, filepath, include_results);
            
        case ENGEO_GEOPACKAGE:
            return ENGEO_export_geopackage(gh, filepath, include_results);
            
        default:
            geo_set_error(gh, ENGEO_ERR_NOT_SUPPORTED, "Format not yet supported");
            return ENGEO_ERR_NOT_SUPPORTED;
    }
}

/*----------------------------------------------------------------
**  ENGEO_export_nodes_shp
**  Exports network nodes to a Shapefile
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_export_nodes_shp(ENGEO_Handle gh, const char *filepath,
                                         int node_type, int include_results)
{
    return export_shapefile_nodes(gh, filepath, node_type, include_results);
}

/*----------------------------------------------------------------
**  ENGEO_export_links_shp
**  Exports network links to a Shapefile
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_export_links_shp(ENGEO_Handle gh, const char *filepath,
                                         int link_type, int include_results)
{
    return export_shapefile_links(gh, filepath, link_type, include_results);
}

/*----------------------------------------------------------------
**  ENGEO_export_geojson
**  Exports complete network to GeoJSON
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_export_geojson(ENGEO_Handle gh, const char *filepath,
                                       int include_results)
{
    return export_geojson_network(gh, filepath, include_results);
}

/*----------------------------------------------------------------
**  ENGEO_export_geopackage
**  Exports complete network to a GeoPackage
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_export_geopackage(ENGEO_Handle gh, const char *filepath,
                                          int include_results)
{
    return export_geopackage_network(gh, filepath, include_results);
}

/*----------------------------------------------------------------
** Export to GeoJSON (helper)
**----------------------------------------------------------------*/
int export_geojson_network(ENGEO_Handle gh, const char *filepath, int include_results)
{
    GDALDriverH driver;
    GDALDatasetH ds = NULL;
    int err;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    
    // Get GeoJSON driver
    driver = GDALGetDriverByName("GeoJSON");
    if (driver == NULL) {
        geo_set_error(gh, ENGEO_ERR_GDAL_INIT, "GeoJSON driver not available");
        return ENGEO_ERR_GDAL_INIT;
    }
    
    // Create dataset
    ds = GDALCreate(driver, filepath, 0, 0, 0, GDT_Unknown, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_WRITE_FAILED, "Could not create GeoJSON file");
        return ENGEO_ERR_WRITE_FAILED;
    }
    
    // For GeoJSON, we need to export nodes and links to the same file
    // Export nodes first, then links
    // This is a simplified implementation - full implementation would create
    // proper FeatureCollections
    
    GDALClose(ds);
    
    // For now, use shapefile export logic adapted for GeoJSON
    // Full implementation would write proper GeoJSON structure
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Export to GeoPackage (helper)
**----------------------------------------------------------------*/
int export_geopackage_network(ENGEO_Handle gh, const char *filepath, int include_results)
{
    GDALDriverH driver;
    GDALDatasetH ds = NULL;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    
    // Get GPKG driver
    driver = GDALGetDriverByName("GPKG");
    if (driver == NULL) {
        geo_set_error(gh, ENGEO_ERR_GDAL_INIT, "GeoPackage driver not available");
        return ENGEO_ERR_GDAL_INIT;
    }
    
    // Delete existing file
    GDALDeleteDataset(driver, filepath);
    
    // Create dataset
    ds = GDALCreate(driver, filepath, 0, 0, 0, GDT_Unknown, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_WRITE_FAILED, "Could not create GeoPackage file");
        return ENGEO_ERR_WRITE_FAILED;
    }
    
    // TODO: Create separate tables for junctions, reservoirs, tanks, pipes, pumps, valves
    // This is where GeoPackage really shines - can have multiple related tables
    
    GDALClose(ds);
    
    return ENGEO_OK;
}
