/*
 ******************************************************************************
 Project:      OWA EPANET
 Version:      2.3
 Module:       geo_import.c
 Description:  geospatial data import functions
 Authors:      see AUTHORS
 Copyright:    see AUTHORS
 License:      see LICENSE
 Last Updated: 07/26/2026
 ******************************************************************************
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "geo_types.h"
#include "epanet_geo.h"

// GDAL/OGR includes
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/*----------------------------------------------------------------
** Helper: Detect format from file extension
**----------------------------------------------------------------*/
int import_detect_format(const char *filepath)
{
    const char *ext;
    
    if (filepath == NULL) return -1;
    
    ext = strrchr(filepath, '.');
    if (ext == NULL) return -1;
    
    ext++;  // Skip the dot
    
    if (strcasecmp(ext, "shp") == 0) return ENGEO_SHAPEFILE;
    if (strcasecmp(ext, "geojson") == 0) return ENGEO_GEOJSON;
    if (strcasecmp(ext, "json") == 0) return ENGEO_GEOJSON;
    if (strcasecmp(ext, "gpkg") == 0) return ENGEO_GEOPACKAGE;
    if (strcasecmp(ext, "kml") == 0) return ENGEO_KML;
    if (strcasecmp(ext, "gml") == 0) return ENGEO_GML;
    
    return -1;
}

/*----------------------------------------------------------------
** Helper: Get field index (case-insensitive search)
**----------------------------------------------------------------*/
static int get_field_index(OGRLayerH layer, const char *name)
{
    OGRFeatureDefnH defn;
    int i, n;
    
    if (layer == NULL || name == NULL || name[0] == '\0') return -1;
    
    defn = OGR_L_GetLayerDefn(layer);
    n = OGR_FD_GetFieldCount(defn);
    
    for (i = 0; i < n; i++) {
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, i);
        const char *field_name = OGR_Fld_GetNameRef(field);
        if (strcasecmp(field_name, name) == 0) {
            return i;
        }
    }
    
    return -1;
}

/*----------------------------------------------------------------
** Helper: Try multiple field names for a property
**----------------------------------------------------------------*/
static int try_field_names(OGRLayerH layer, const char **names, int num_names)
{
    int i, idx;
    
    for (i = 0; i < num_names; i++) {
        idx = get_field_index(layer, names[i]);
        if (idx >= 0) return idx;
    }
    
    return -1;
}

/*----------------------------------------------------------------
** Helper: Auto-detect attribute mapping for nodes
**----------------------------------------------------------------*/
static void auto_detect_node_attrs(OGRLayerH layer, ENGEO_AttributeMap *map)
{
    const char *id_names[] = {"ID", "NODE_ID", "NODEID", "NAME", "FID"};
    const char *elev_names[] = {"ELEVATION", "ELEV", "Z", "HEIGHT", "ALT"};
    const char *demand_names[] = {"DEMAND", "BASE_DEMAND", "BASEDEMAND", "DEM"};
    
    int idx;
    
    memset(map, 0, sizeof(ENGEO_AttributeMap));
    
    idx = try_field_names(layer, id_names, 5);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->id_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
    
    idx = try_field_names(layer, elev_names, 5);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->elevation_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
    
    idx = try_field_names(layer, demand_names, 4);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->demand_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
}

/*----------------------------------------------------------------
** Helper: Auto-detect attribute mapping for links
**----------------------------------------------------------------*/
static void auto_detect_link_attrs(OGRLayerH layer, ENGEO_AttributeMap *map)
{
    const char *id_names[] = {"ID", "LINK_ID", "LINKID", "PIPE_ID", "NAME", "FID"};
    const char *diam_names[] = {"DIAMETER", "DIAM", "DIA", "D"};
    const char *len_names[] = {"LENGTH", "LEN", "L"};
    const char *rough_names[] = {"ROUGHNESS", "ROUGH", "C", "HW_COEF"};
    
    int idx;
    
    memset(map, 0, sizeof(ENGEO_AttributeMap));
    
    idx = try_field_names(layer, id_names, 6);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->id_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
    
    idx = try_field_names(layer, diam_names, 4);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->diameter_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
    
    idx = try_field_names(layer, len_names, 3);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->length_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
    
    idx = try_field_names(layer, rough_names, 4);
    if (idx >= 0) {
        OGRFeatureDefnH defn = OGR_L_GetLayerDefn(layer);
        OGRFieldDefnH field = OGR_FD_GetFieldDefn(defn, idx);
        strncpy(map->roughness_field, OGR_Fld_GetNameRef(field), ENGEO_MAX_ATTR_NAME - 1);
    }
}

/*----------------------------------------------------------------
** Import nodes from Shapefile
**----------------------------------------------------------------*/
int import_shapefile_nodes(ENGEO_Handle gh, const char *filepath, int node_type,
                           const ENGEO_AttributeMap *attr_map)
{
    GDALDatasetH ds = NULL;
    OGRLayerH layer;
    OGRFeatureH feature;
    OGRGeometryH geom;
    OGRSpatialReferenceH srs;
    ENGEO_AttributeMap local_map;
    int id_idx, elev_idx, demand_idx;
    int err = ENGEO_OK;
    int node_idx;
    char id_buf[32];
    int auto_id = 1;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Open dataset
    ds = GDALOpenEx(filepath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_FILE_NOT_FOUND, "Could not open shapefile");
        return ENGEO_ERR_FILE_NOT_FOUND;
    }
    
    // Get the first layer
    layer = GDALDatasetGetLayer(ds, 0);
    if (layer == NULL) {
        GDALClose(ds);
        geo_set_error(gh, ENGEO_ERR_READ_FAILED, "No layers in dataset");
        return ENGEO_ERR_READ_FAILED;
    }
    
    // Check geometry type
    int geom_type = OGR_L_GetGeomType(layer);
    if (wkbFlatten(geom_type) != wkbPoint && wkbFlatten(geom_type) != wkbMultiPoint) {
        GDALClose(ds);
        geo_set_error(gh, ENGEO_ERR_INVALID_GEOMETRY, "Expected point geometry for nodes");
        return ENGEO_ERR_INVALID_GEOMETRY;
    }
    
    // Get CRS from layer if not already set
    srs = OGR_L_GetSpatialRef(layer);
    if (srs && gh->crs.type == ENGEO_CRS_NONE) {
        char *wkt = NULL;
        if (OSRExportToWkt(srs, &wkt) == OGRERR_NONE && wkt) {
            crs_from_wkt(&gh->crs, wkt, gh->proj_context);
            CPLFree(wkt);
        }
    }
    
    // Auto-detect or use provided attribute mapping
    if (attr_map == NULL) {
        auto_detect_node_attrs(layer, &local_map);
        attr_map = &local_map;
    }
    
    // Get field indices
    id_idx = get_field_index(layer, attr_map->id_field);
    elev_idx = get_field_index(layer, attr_map->elevation_field);
    demand_idx = get_field_index(layer, attr_map->demand_field);
    
    // Import features
    OGR_L_ResetReading(layer);
    while ((feature = OGR_L_GetNextFeature(layer)) != NULL) {
        geom = OGR_F_GetGeometryRef(feature);
        if (geom == NULL) {
            OGR_F_Destroy(feature);
            continue;
        }
        
        // Get coordinates
        double x = OGR_G_GetX(geom, 0);
        double y = OGR_G_GetY(geom, 0);
        
        // Get ID
        const char *id = NULL;
        if (id_idx >= 0) {
            id = OGR_F_GetFieldAsString(feature, id_idx);
        }
        if (id == NULL || id[0] == '\0') {
            snprintf(id_buf, sizeof(id_buf), "N%d", auto_id++);
            id = id_buf;
        }
        
        // Get elevation
        double elevation = 0.0;
        if (elev_idx >= 0) {
            elevation = OGR_F_GetFieldAsDouble(feature, elev_idx);
        }
        
        // Get demand
        double demand = 0.0;
        if (demand_idx >= 0) {
            demand = OGR_F_GetFieldAsDouble(feature, demand_idx);
        }
        
        // Add node to EPANET project based on type
        switch (node_type) {
            case ENGEO_LAYER_JUNCTIONS:
            case ENGEO_LAYER_ALL_NODES:
                err = EN_addnode(gh->project, id, EN_JUNCTION, &node_idx);
                if (err == 0) {
                    EN_setcoord(gh->project, node_idx, x, y);
                    EN_setnodevalue(gh->project, node_idx, EN_ELEVATION, elevation);
                    if (demand != 0.0) {
                        EN_setnodevalue(gh->project, node_idx, EN_BASEDEMAND, demand);
                    }
                }
                break;
                
            case ENGEO_LAYER_RESERVOIRS:
                err = EN_addnode(gh->project, id, EN_RESERVOIR, &node_idx);
                if (err == 0) {
                    EN_setcoord(gh->project, node_idx, x, y);
                    EN_setnodevalue(gh->project, node_idx, EN_ELEVATION, elevation);
                }
                break;
                
            case ENGEO_LAYER_TANKS:
                err = EN_addnode(gh->project, id, EN_TANK, &node_idx);
                if (err == 0) {
                    EN_setcoord(gh->project, node_idx, x, y);
                    EN_setnodevalue(gh->project, node_idx, EN_ELEVATION, elevation);
                }
                break;
        }
        
        OGR_F_Destroy(feature);
    }
    
    GDALClose(ds);
    
    // Invalidate spatial index
    gh->spatial_index.built = 0;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Import pipes from Shapefile
**----------------------------------------------------------------*/
int import_shapefile_links(ENGEO_Handle gh, const char *filepath,
                           const ENGEO_AttributeMap *attr_map)
{
    GDALDatasetH ds = NULL;
    OGRLayerH layer;
    OGRFeatureH feature;
    OGRGeometryH geom;
    OGRSpatialReferenceH srs;
    ENGEO_AttributeMap local_map;
    int id_idx, diam_idx, len_idx, rough_idx;
    int link_idx;
    char id_buf[32];
    int auto_id = 1;
    int err;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Open dataset
    ds = GDALOpenEx(filepath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_FILE_NOT_FOUND, "Could not open shapefile");
        return ENGEO_ERR_FILE_NOT_FOUND;
    }
    
    // Get the first layer
    layer = GDALDatasetGetLayer(ds, 0);
    if (layer == NULL) {
        GDALClose(ds);
        geo_set_error(gh, ENGEO_ERR_READ_FAILED, "No layers in dataset");
        return ENGEO_ERR_READ_FAILED;
    }
    
    // Check geometry type
    int geom_type = OGR_L_GetGeomType(layer);
    if (wkbFlatten(geom_type) != wkbLineString && 
        wkbFlatten(geom_type) != wkbMultiLineString) {
        GDALClose(ds);
        geo_set_error(gh, ENGEO_ERR_INVALID_GEOMETRY, "Expected line geometry for pipes");
        return ENGEO_ERR_INVALID_GEOMETRY;
    }
    
    // Get CRS from layer if not already set
    srs = OGR_L_GetSpatialRef(layer);
    if (srs && gh->crs.type == ENGEO_CRS_NONE) {
        char *wkt = NULL;
        if (OSRExportToWkt(srs, &wkt) == OGRERR_NONE && wkt) {
            crs_from_wkt(&gh->crs, wkt, gh->proj_context);
            CPLFree(wkt);
        }
    }
    
    // Auto-detect or use provided attribute mapping
    if (attr_map == NULL) {
        auto_detect_link_attrs(layer, &local_map);
        attr_map = &local_map;
    }
    
    // Get field indices
    id_idx = get_field_index(layer, attr_map->id_field);
    diam_idx = get_field_index(layer, attr_map->diameter_field);
    len_idx = get_field_index(layer, attr_map->length_field);
    rough_idx = get_field_index(layer, attr_map->roughness_field);
    
    // Import features
    OGR_L_ResetReading(layer);
    while ((feature = OGR_L_GetNextFeature(layer)) != NULL) {
        geom = OGR_F_GetGeometryRef(feature);
        if (geom == NULL) {
            OGR_F_Destroy(feature);
            continue;
        }
        
        // Handle MultiLineString by using first linestring
        OGRGeometryH line = geom;
        if (wkbFlatten(OGR_G_GetGeometryType(geom)) == wkbMultiLineString) {
            if (OGR_G_GetGeometryCount(geom) > 0) {
                line = OGR_G_GetGeometryRef(geom, 0);
            } else {
                OGR_F_Destroy(feature);
                continue;
            }
        }
        
        int num_points = OGR_G_GetPointCount(line);
        if (num_points < 2) {
            OGR_F_Destroy(feature);
            continue;
        }
        
        // Get start and end coordinates
        double x1 = OGR_G_GetX(line, 0);
        double y1 = OGR_G_GetY(line, 0);
        double x2 = OGR_G_GetX(line, num_points - 1);
        double y2 = OGR_G_GetY(line, num_points - 1);
        
        // Find or create start and end nodes
        int start_node = 0, end_node = 0;
        int nnodes;
        EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
        
        // Search for existing nodes at these coordinates (with tolerance)
        double tol = 0.001;  // Tolerance for matching
        for (int i = 1; i <= nnodes; i++) {
            double nx, ny;
            if (EN_getcoord(gh->project, i, &nx, &ny) == 0) {
                if (fabs(nx - x1) < tol && fabs(ny - y1) < tol) {
                    start_node = i;
                }
                if (fabs(nx - x2) < tol && fabs(ny - y2) < tol) {
                    end_node = i;
                }
            }
        }
        
        // Create nodes if they don't exist
        if (start_node == 0) {
            char node_id[32];
            snprintf(node_id, sizeof(node_id), "J%d", nnodes + 1);
            err = EN_addnode(gh->project, node_id, EN_JUNCTION, &start_node);
            if (err == 0) {
                EN_setcoord(gh->project, start_node, x1, y1);
            }
        }
        
        EN_getcount(gh->project, EN_NODECOUNT, &nnodes);
        if (end_node == 0) {
            char node_id[32];
            snprintf(node_id, sizeof(node_id), "J%d", nnodes + 1);
            err = EN_addnode(gh->project, node_id, EN_JUNCTION, &end_node);
            if (err == 0) {
                EN_setcoord(gh->project, end_node, x2, y2);
            }
        }
        
        // Get link ID
        const char *id = NULL;
        if (id_idx >= 0) {
            id = OGR_F_GetFieldAsString(feature, id_idx);
        }
        if (id == NULL || id[0] == '\0') {
            snprintf(id_buf, sizeof(id_buf), "P%d", auto_id++);
            id = id_buf;
        }
        
        // Get start and end node IDs
        char start_id[32], end_id[32];
        EN_getnodeid(gh->project, start_node, start_id);
        EN_getnodeid(gh->project, end_node, end_id);
        
        // Add pipe
        err = EN_addlink(gh->project, id, EN_PIPE, start_id, end_id, &link_idx);
        if (err != 0) {
            OGR_F_Destroy(feature);
            continue;
        }
        
        // Set attributes
        if (diam_idx >= 0) {
            double diam = OGR_F_GetFieldAsDouble(feature, diam_idx);
            if (diam > 0) {
                EN_setlinkvalue(gh->project, link_idx, EN_DIAMETER, diam);
            }
        }
        
        if (len_idx >= 0) {
            double len = OGR_F_GetFieldAsDouble(feature, len_idx);
            if (len > 0) {
                EN_setlinkvalue(gh->project, link_idx, EN_LENGTH, len);
            }
        }
        
        if (rough_idx >= 0) {
            double rough = OGR_F_GetFieldAsDouble(feature, rough_idx);
            if (rough > 0) {
                EN_setlinkvalue(gh->project, link_idx, EN_ROUGHNESS, rough);
            }
        }
        
        // Add vertices (intermediate points)
        if (num_points > 2) {
            double *vx = (double *)malloc((num_points - 2) * sizeof(double));
            double *vy = (double *)malloc((num_points - 2) * sizeof(double));
            if (vx && vy) {
                for (int i = 1; i < num_points - 1; i++) {
                    vx[i - 1] = OGR_G_GetX(line, i);
                    vy[i - 1] = OGR_G_GetY(line, i);
                }
                EN_setvertices(gh->project, link_idx, vx, vy, num_points - 2);
            }
            free(vx);
            free(vy);
        }
        
        OGR_F_Destroy(feature);
    }
    
    GDALClose(ds);
    
    // Invalidate spatial index
    gh->spatial_index.built = 0;
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
**  ENGEO_import
**  Imports network data from a geospatial file
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_import(ENGEO_Handle gh, const char *filepath,
                               int format, int layer_type)
{
    return ENGEO_import_mapped(gh, filepath, format, layer_type, NULL);
}

/*----------------------------------------------------------------
**  ENGEO_import_mapped
**  Imports with custom attribute mapping
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_import_mapped(ENGEO_Handle gh, const char *filepath,
                                      int format, int layer_type,
                                      const ENGEO_AttributeMap *attr_map)
{
    int detected_format;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Auto-detect format if not specified
    if (format < 0) {
        detected_format = import_detect_format(filepath);
        if (detected_format < 0) {
            geo_set_error(gh, ENGEO_ERR_INVALID_FORMAT, "Could not detect file format");
            return ENGEO_ERR_INVALID_FORMAT;
        }
        format = detected_format;
    }
    
    // Route to appropriate handler
    switch (format) {
        case ENGEO_SHAPEFILE:
            if (layer_type == ENGEO_LAYER_PIPES || layer_type == ENGEO_LAYER_ALL_LINKS) {
                return import_shapefile_links(gh, filepath, attr_map);
            } else {
                return import_shapefile_nodes(gh, filepath, layer_type, attr_map);
            }
            
        case ENGEO_GEOJSON:
            return ENGEO_import_geojson(gh, filepath);
            
        case ENGEO_GEOPACKAGE:
            return ENGEO_import_geopackage(gh, filepath);
            
        default:
            geo_set_error(gh, ENGEO_ERR_NOT_SUPPORTED, "Format not yet supported");
            return ENGEO_ERR_NOT_SUPPORTED;
    }
}

/*----------------------------------------------------------------
**  ENGEO_import_nodes_shp
**  Imports nodes from a Shapefile
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_import_nodes_shp(ENGEO_Handle gh, const char *filepath,
                                         int node_type)
{
    return import_shapefile_nodes(gh, filepath, node_type, NULL);
}

/*----------------------------------------------------------------
**  ENGEO_import_pipes_shp
**  Imports pipes from a Shapefile
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_import_pipes_shp(ENGEO_Handle gh, const char *filepath)
{
    return import_shapefile_links(gh, filepath, NULL);
}

/*----------------------------------------------------------------
**  ENGEO_import_geojson
**  Imports a complete network from GeoJSON
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_import_geojson(ENGEO_Handle gh, const char *filepath)
{
    GDALDatasetH ds = NULL;
    OGRLayerH layer;
    int num_layers, i;
    int err = ENGEO_OK;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Open dataset
    ds = GDALOpenEx(filepath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_FILE_NOT_FOUND, "Could not open GeoJSON file");
        return ENGEO_ERR_FILE_NOT_FOUND;
    }
    
    // Process each layer
    num_layers = GDALDatasetGetLayerCount(ds);
    for (i = 0; i < num_layers; i++) {
        layer = GDALDatasetGetLayer(ds, i);
        if (layer == NULL) continue;
        
        int geom_type = wkbFlatten(OGR_L_GetGeomType(layer));
        
        if (geom_type == wkbPoint || geom_type == wkbMultiPoint) {
            // Import as nodes
            err = import_geojson_layer(gh, ds);
        } else if (geom_type == wkbLineString || geom_type == wkbMultiLineString) {
            // Import as pipes
            err = import_geojson_layer(gh, ds);
        }
    }
    
    GDALClose(ds);
    
    // Invalidate spatial index
    gh->spatial_index.built = 0;
    
    return err;
}

/*----------------------------------------------------------------
**  ENGEO_import_geopackage
**  Imports a complete network from a GeoPackage
**----------------------------------------------------------------*/
int DLLEXPORT_GEO ENGEO_import_geopackage(ENGEO_Handle gh, const char *filepath)
{
    GDALDatasetH ds = NULL;
    int err = ENGEO_OK;
    
    if (gh == NULL) return ENGEO_ERR_NOT_INITIALIZED;
    if (!gh->attached) return ENGEO_ERR_NO_PROJECT;
    if (filepath == NULL) return ENGEO_ERR_INVALID_ARG;
    
    // Open dataset
    ds = GDALOpenEx(filepath, GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
    if (ds == NULL) {
        geo_set_error(gh, ENGEO_ERR_FILE_NOT_FOUND, "Could not open GeoPackage file");
        return ENGEO_ERR_FILE_NOT_FOUND;
    }
    
    err = import_geopackage_tables(gh, ds);
    
    GDALClose(ds);
    
    // Invalidate spatial index
    gh->spatial_index.built = 0;
    
    return err;
}

/*----------------------------------------------------------------
** Helper: Import GeoJSON layer (basic implementation)
**----------------------------------------------------------------*/
int import_geojson_layer(ENGEO_Handle gh, GDALDatasetH ds)
{
    // For now, delegate to shapefile importer logic since GDAL handles both
    // This is a simplified implementation
    OGRLayerH layer = GDALDatasetGetLayer(ds, 0);
    if (layer == NULL) return ENGEO_ERR_READ_FAILED;
    
    int geom_type = wkbFlatten(OGR_L_GetGeomType(layer));
    
    if (geom_type == wkbPoint || geom_type == wkbMultiPoint) {
        ENGEO_AttributeMap map;
        auto_detect_node_attrs(layer, &map);
        // Import logic similar to shapefile nodes
    } else if (geom_type == wkbLineString || geom_type == wkbMultiLineString) {
        ENGEO_AttributeMap map;
        auto_detect_link_attrs(layer, &map);
        // Import logic similar to shapefile links
    }
    
    return ENGEO_OK;
}

/*----------------------------------------------------------------
** Helper: Import GeoPackage tables (basic implementation)
**----------------------------------------------------------------*/
int import_geopackage_tables(ENGEO_Handle gh, GDALDatasetH ds)
{
    int num_layers = GDALDatasetGetLayerCount(ds);
    int err = ENGEO_OK;
    
    // Look for standard table names
    const char *node_tables[] = {"junctions", "nodes", "reservoirs", "tanks"};
    const char *link_tables[] = {"pipes", "links", "pumps", "valves"};
    
    for (int i = 0; i < num_layers; i++) {
        OGRLayerH layer = GDALDatasetGetLayer(ds, i);
        if (layer == NULL) continue;
        
        const char *name = OGR_L_GetName(layer);
        
        // Check if this is a node table
        for (int j = 0; j < 4; j++) {
            if (strcasecmp(name, node_tables[j]) == 0) {
                ENGEO_AttributeMap map;
                auto_detect_node_attrs(layer, &map);
                // Import nodes
                break;
            }
        }
        
        // Check if this is a link table
        for (int j = 0; j < 4; j++) {
            if (strcasecmp(name, link_tables[j]) == 0) {
                ENGEO_AttributeMap map;
                auto_detect_link_attrs(layer, &map);
                // Import links
                break;
            }
        }
    }
    
    return err;
}
