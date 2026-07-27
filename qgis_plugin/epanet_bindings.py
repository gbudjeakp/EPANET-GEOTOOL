# -*- coding: utf-8 -*-
"""
Python bindings for EPANET and epanet-geo libraries using ctypes
"""

import os
import sys
import ctypes
from ctypes import (
    c_int, c_double, c_char_p, c_void_p, c_long,
    POINTER, byref, create_string_buffer
)

# Determine library extension based on platform
if sys.platform == 'darwin':
    LIB_EXT = '.dylib'
elif sys.platform == 'win32':
    LIB_EXT = '.dll'
else:
    LIB_EXT = '.so'

# Find library paths
def _find_library(name):
    """Find library in common locations."""
    plugin_dir = os.path.dirname(__file__)
    
    search_paths = [
        os.path.join(plugin_dir, 'lib'),
        os.path.join(plugin_dir, '..', 'lib'),
        '/usr/local/lib',
        '/opt/homebrew/lib',
        os.path.expanduser('~/lib'),
    ]
    
    for path in search_paths:
        lib_path = os.path.join(path, f'lib{name}{LIB_EXT}')
        if os.path.exists(lib_path):
            return lib_path
    
    # Return None if not found
    return None


# -----------------------------------------------------------------------------
# EPANET Core Bindings
# -----------------------------------------------------------------------------

class EpanetProject:
    """Python wrapper for EPANET project."""
    
    # Class-level library reference
    _lib = None
    _lib_loaded = False
    _lib_error = None
    
    @classmethod
    def _load_lib(cls):
        """Load the EPANET library."""
        if cls._lib_loaded:
            if cls._lib_error:
                raise RuntimeError(cls._lib_error)
            return cls._lib
            
        cls._lib_loaded = True
        lib_path = _find_library('epanet2')
        if lib_path is None:
            cls._lib_error = "EPANET library (libepanet2) not found. Please build EPANET first."
            raise RuntimeError(cls._lib_error)
        try:
            cls._lib = ctypes.CDLL(lib_path)
            cls._setup_functions()
        except Exception as e:
            cls._lib_error = f"Failed to load EPANET library: {e}"
            raise RuntimeError(cls._lib_error)
        return cls._lib
    
    @classmethod
    def _setup_functions(cls):
        """Set up function signatures."""
        lib = cls._lib
        
        # EN_createproject
        lib.EN_createproject.argtypes = [POINTER(c_void_p)]
        lib.EN_createproject.restype = c_int
        
        # EN_deleteproject
        lib.EN_deleteproject.argtypes = [c_void_p]
        lib.EN_deleteproject.restype = c_int
        
        # EN_open
        lib.EN_open.argtypes = [c_void_p, c_char_p, c_char_p, c_char_p]
        lib.EN_open.restype = c_int
        
        # EN_saveinpfile
        lib.EN_saveinpfile.argtypes = [c_void_p, c_char_p]
        lib.EN_saveinpfile.restype = c_int
        
        # EN_close
        lib.EN_close.argtypes = [c_void_p]
        lib.EN_close.restype = c_int
        
        # EN_getcount
        lib.EN_getcount.argtypes = [c_void_p, c_int, POINTER(c_int)]
        lib.EN_getcount.restype = c_int
        
        # EN_getnodeid
        lib.EN_getnodeid.argtypes = [c_void_p, c_int, c_char_p]
        lib.EN_getnodeid.restype = c_int
        
        # EN_getnodetype
        lib.EN_getnodetype.argtypes = [c_void_p, c_int, POINTER(c_int)]
        lib.EN_getnodetype.restype = c_int
        
        # EN_getnodevalue
        lib.EN_getnodevalue.argtypes = [c_void_p, c_int, c_int, POINTER(c_double)]
        lib.EN_getnodevalue.restype = c_int
        
        # EN_getcoord
        lib.EN_getcoord.argtypes = [c_void_p, c_int, POINTER(c_double), POINTER(c_double)]
        lib.EN_getcoord.restype = c_int
        
        # EN_setcoord
        lib.EN_setcoord.argtypes = [c_void_p, c_int, c_double, c_double]
        lib.EN_setcoord.restype = c_int
        
        # EN_getlinkid
        lib.EN_getlinkid.argtypes = [c_void_p, c_int, c_char_p]
        lib.EN_getlinkid.restype = c_int
        
        # EN_getlinktype
        lib.EN_getlinktype.argtypes = [c_void_p, c_int, POINTER(c_int)]
        lib.EN_getlinktype.restype = c_int
        
        # EN_getlinknodes
        lib.EN_getlinknodes.argtypes = [c_void_p, c_int, POINTER(c_int), POINTER(c_int)]
        lib.EN_getlinknodes.restype = c_int
        
        # EN_getlinkvalue
        lib.EN_getlinkvalue.argtypes = [c_void_p, c_int, c_int, POINTER(c_double)]
        lib.EN_getlinkvalue.restype = c_int
        
        # EN_getvertexcount
        lib.EN_getvertexcount.argtypes = [c_void_p, c_int, POINTER(c_int)]
        lib.EN_getvertexcount.restype = c_int
        
        # EN_getvertex
        lib.EN_getvertex.argtypes = [c_void_p, c_int, c_int, POINTER(c_double), POINTER(c_double)]
        lib.EN_getvertex.restype = c_int
        
        # Hydraulic analysis functions
        lib.EN_openH.argtypes = [c_void_p]
        lib.EN_openH.restype = c_int
        
        lib.EN_initH.argtypes = [c_void_p, c_int]
        lib.EN_initH.restype = c_int
        
        lib.EN_runH.argtypes = [c_void_p, POINTER(c_long)]
        lib.EN_runH.restype = c_int
        
        lib.EN_nextH.argtypes = [c_void_p, POINTER(c_long)]
        lib.EN_nextH.restype = c_int
        
        lib.EN_closeH.argtypes = [c_void_p]
        lib.EN_closeH.restype = c_int
        
        # Error message
        lib.EN_geterror.argtypes = [c_int, c_char_p, c_int]
        lib.EN_geterror.restype = c_int

    def __init__(self):
        """Initialize EPANET project."""
        self._load_lib()
        self._handle = c_void_p()
        self._is_open = False
        self._created = False
    
    def create(self):
        """Create a new empty project."""
        if self._created:
            return 0
        err = self._lib.EN_createproject(byref(self._handle))
        if err == 0:
            self._created = True
        return err
    
    def open(self, inp_file, rpt_file='', out_file=''):
        """Open an INP file."""
        # Must create project first
        if not self._created:
            err = self.create()
            if err != 0:
                return err
        err = self._lib.EN_open(
            self._handle,
            inp_file.encode('utf-8'),
            rpt_file.encode('utf-8'),
            out_file.encode('utf-8')
        )
        if err == 0:
            self._is_open = True
        return err
    
    def save(self, filename):
        """Save to INP file."""
        return self._lib.EN_saveinpfile(
            self._handle, filename.encode('utf-8')
        )
    
    def close(self):
        """Close the project."""
        if self._is_open:
            self._lib.EN_close(self._handle)
            self._is_open = False
        if self._created and self._handle:
            self._lib.EN_deleteproject(self._handle)
            self._handle = c_void_p()
            self._created = False
    
    def get_handle(self):
        """Get the raw handle for passing to epanet-geo."""
        return self._handle
    
    def get_count(self, count_type):
        """Get count of nodes/links/etc."""
        count = c_int()
        self._lib.EN_getcount(self._handle, count_type, byref(count))
        return count.value
    
    def get_node_id(self, index):
        """Get node ID string."""
        buf = create_string_buffer(32)
        self._lib.EN_getnodeid(self._handle, index, buf)
        return buf.value.decode('utf-8')
    
    def get_node_type(self, index):
        """Get node type."""
        ntype = c_int()
        self._lib.EN_getnodetype(self._handle, index, byref(ntype))
        return ntype.value
    
    def get_node_value(self, index, param):
        """Get a node parameter value."""
        value = c_double()
        self._lib.EN_getnodevalue(self._handle, index, param, byref(value))
        return value.value
    
    def get_coord(self, index):
        """Get node coordinates."""
        x = c_double()
        y = c_double()
        self._lib.EN_getcoord(self._handle, index, byref(x), byref(y))
        return x.value, y.value
    
    def set_coord(self, index, x, y):
        """Set node coordinates."""
        return self._lib.EN_setcoord(self._handle, index, c_double(x), c_double(y))
    
    def get_link_id(self, index):
        """Get link ID string."""
        buf = create_string_buffer(32)
        self._lib.EN_getlinkid(self._handle, index, buf)
        return buf.value.decode('utf-8')
    
    def get_link_type(self, index):
        """Get link type."""
        ltype = c_int()
        self._lib.EN_getlinktype(self._handle, index, byref(ltype))
        return ltype.value
    
    def get_link_nodes(self, index):
        """Get the two nodes of a link."""
        n1 = c_int()
        n2 = c_int()
        self._lib.EN_getlinknodes(self._handle, index, byref(n1), byref(n2))
        return n1.value, n2.value
    
    def get_link_value(self, index, param):
        """Get a link parameter value."""
        value = c_double()
        self._lib.EN_getlinkvalue(self._handle, index, param, byref(value))
        return value.value
    
    def get_vertex_count(self, link_index):
        """Get number of vertices on a link."""
        count = c_int()
        self._lib.EN_getvertexcount(self._handle, link_index, byref(count))
        return count.value
    
    def get_vertex(self, link_index, vertex_index):
        """Get vertex coordinates."""
        x = c_double()
        y = c_double()
        self._lib.EN_getvertex(self._handle, link_index, vertex_index, byref(x), byref(y))
        return x.value, y.value
    
    def run_hydraulics(self):
        """Run a single-period hydraulic analysis."""
        err = self._lib.EN_openH(self._handle)
        if err != 0:
            return err
        
        err = self._lib.EN_initH(self._handle, 0)
        if err != 0:
            self._lib.EN_closeH(self._handle)
            return err
        
        t = c_long()
        err = self._lib.EN_runH(self._handle, byref(t))
        
        self._lib.EN_closeH(self._handle)
        return err
    
    def get_error(self, errcode):
        """Get error message for an error code."""
        buf = create_string_buffer(256)
        self._lib.EN_geterror(errcode, buf, 256)
        return buf.value.decode('utf-8')


# -----------------------------------------------------------------------------
# EPANET-Geo Bindings
# -----------------------------------------------------------------------------

class EpanetGeo:
    """Python wrapper for epanet-geo library."""
    
    _lib = None
    _lib_loaded = False
    _lib_error = None
    
    @classmethod
    def _load_lib(cls):
        """Load the epanet-geo library."""
        if cls._lib_loaded:
            if cls._lib_error:
                raise RuntimeError(cls._lib_error)
            return cls._lib
            
        cls._lib_loaded = True
        lib_path = _find_library('epanet-geo')
        if lib_path is None:
            cls._lib_error = "epanet-geo library not found. Please build with -DBUILD_GEO=ON."
            raise RuntimeError(cls._lib_error)
        try:
            cls._lib = ctypes.CDLL(lib_path)
            cls._setup_functions()
        except Exception as e:
            cls._lib_error = f"Failed to load epanet-geo library: {e}"
            raise RuntimeError(cls._lib_error)
        return cls._lib
    
    @classmethod
    def _setup_functions(cls):
        """Set up function signatures."""
        lib = cls._lib
        
        # Context management
        lib.ENGEO_create.argtypes = [POINTER(c_void_p)]
        lib.ENGEO_create.restype = c_int
        
        lib.ENGEO_destroy.argtypes = [c_void_p]
        lib.ENGEO_destroy.restype = c_int
        
        lib.ENGEO_attach.argtypes = [c_void_p, c_void_p]
        lib.ENGEO_attach.restype = c_int
        
        lib.ENGEO_detach.argtypes = [c_void_p]
        lib.ENGEO_detach.restype = c_int
        
        # CRS functions
        lib.ENGEO_setcrs_epsg.argtypes = [c_void_p, c_int]
        lib.ENGEO_setcrs_epsg.restype = c_int
        
        lib.ENGEO_getcrs_epsg.argtypes = [c_void_p, POINTER(c_int)]
        lib.ENGEO_getcrs_epsg.restype = c_int
        
        lib.ENGEO_transform_network.argtypes = [c_void_p, c_int]
        lib.ENGEO_transform_network.restype = c_int
        
        # Import functions
        lib.ENGEO_import_nodes_shp.argtypes = [c_void_p, c_char_p, c_int, c_void_p]
        lib.ENGEO_import_nodes_shp.restype = c_int
        
        lib.ENGEO_import_pipes_shp.argtypes = [c_void_p, c_char_p, c_void_p]
        lib.ENGEO_import_pipes_shp.restype = c_int
        
        lib.ENGEO_import_geojson.argtypes = [c_void_p, c_char_p]
        lib.ENGEO_import_geojson.restype = c_int
        
        # Export functions
        lib.ENGEO_export_nodes_shp.argtypes = [c_void_p, c_char_p, c_int, c_int]
        lib.ENGEO_export_nodes_shp.restype = c_int
        
        lib.ENGEO_export_links_shp.argtypes = [c_void_p, c_char_p, c_int, c_int]
        lib.ENGEO_export_links_shp.restype = c_int
        
        lib.ENGEO_export_geojson.argtypes = [c_void_p, c_char_p, c_int]
        lib.ENGEO_export_geojson.restype = c_int
        
        # DEM functions
        lib.ENGEO_open_dem.argtypes = [c_void_p, c_char_p]
        lib.ENGEO_open_dem.restype = c_int
        
        lib.ENGEO_close_dem.argtypes = [c_void_p]
        lib.ENGEO_close_dem.restype = c_int
        
        lib.ENGEO_assign_elevations.argtypes = [c_void_p, c_int]
        lib.ENGEO_assign_elevations.restype = c_int
        
        # Utility functions
        lib.ENGEO_calc_pipe_lengths.argtypes = [c_void_p, c_int]
        lib.ENGEO_calc_pipe_lengths.restype = c_int
        
        lib.ENGEO_geterror.argtypes = [c_void_p, c_char_p, c_int]
        lib.ENGEO_geterror.restype = c_int

    def __init__(self):
        """Initialize geo context."""
        self._load_lib()
        self._handle = c_void_p()
        self._created = False
    
    def create(self):
        """Create geo context."""
        if self._created:
            return 0
        err = self._lib.ENGEO_create(byref(self._handle))
        if err == 0:
            self._created = True
        return err
    
    def destroy(self):
        """Destroy geo context."""
        if self._created and self._handle.value:
            self._lib.ENGEO_destroy(byref(self._handle))
            self._created = False
        self._handle = c_void_p()
    
    def attach(self, project):
        """Attach to an EPANET project."""
        return self._lib.ENGEO_attach(self._handle, project.get_handle())
    
    def detach(self):
        """Detach from EPANET project."""
        return self._lib.ENGEO_detach(self._handle)
    
    def set_crs_epsg(self, epsg):
        """Set CRS by EPSG code."""
        return self._lib.ENGEO_setcrs_epsg(self._handle, epsg)
    
    def get_crs_epsg(self):
        """Get current EPSG code."""
        epsg = c_int()
        self._lib.ENGEO_getcrs_epsg(self._handle, byref(epsg))
        return epsg.value
    
    def transform_network(self, target_epsg):
        """Transform network coordinates to new CRS."""
        return self._lib.ENGEO_transform_network(self._handle, target_epsg)
    
    def import_nodes_shp(self, filepath, node_type, attr_map=None):
        """Import nodes from shapefile."""
        return self._lib.ENGEO_import_nodes_shp(
            self._handle,
            filepath.encode('utf-8'),
            node_type,
            attr_map or None
        )
    
    def import_pipes_shp(self, filepath, attr_map=None):
        """Import pipes from shapefile."""
        return self._lib.ENGEO_import_pipes_shp(
            self._handle,
            filepath.encode('utf-8'),
            attr_map or None
        )
    
    def import_geojson(self, filepath):
        """Import network from GeoJSON."""
        return self._lib.ENGEO_import_geojson(
            self._handle,
            filepath.encode('utf-8')
        )
    
    def export_nodes_shp(self, filepath, node_type, include_results=False):
        """Export nodes to shapefile."""
        return self._lib.ENGEO_export_nodes_shp(
            self._handle,
            filepath.encode('utf-8'),
            node_type,
            1 if include_results else 0
        )
    
    def export_links_shp(self, filepath, link_type, include_results=False):
        """Export links to shapefile."""
        return self._lib.ENGEO_export_links_shp(
            self._handle,
            filepath.encode('utf-8'),
            link_type,
            1 if include_results else 0
        )
    
    def export_geojson(self, filepath, export_mode=0):
        """Export network to GeoJSON."""
        return self._lib.ENGEO_export_geojson(
            self._handle,
            filepath.encode('utf-8'),
            export_mode
        )
    
    def open_dem(self, filepath):
        """Open a DEM file."""
        return self._lib.ENGEO_open_dem(
            self._handle,
            filepath.encode('utf-8')
        )
    
    def close_dem(self):
        """Close the DEM."""
        return self._lib.ENGEO_close_dem(self._handle)
    
    def assign_elevations(self, method=1):
        """Assign elevations from DEM. Method: 0=nearest, 1=bilinear."""
        return self._lib.ENGEO_assign_elevations(self._handle, method)
    
    def calc_pipe_lengths(self, update=True):
        """Calculate pipe lengths from geometry."""
        return self._lib.ENGEO_calc_pipe_lengths(self._handle, 1 if update else 0)
    
    def get_error(self):
        """Get last error message."""
        buf = create_string_buffer(256)
        self._lib.ENGEO_geterror(self._handle, buf, 256)
        return buf.value.decode('utf-8')
