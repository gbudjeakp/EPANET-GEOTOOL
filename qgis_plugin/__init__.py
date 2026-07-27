# -*- coding: utf-8 -*-
"""
EPANET GeoTools - QGIS Plugin
Geospatial modeling for EPANET water distribution networks
"""


def classFactory(iface):
    """Load EpanetGeoPlugin class from file epanet_geo_plugin.
    
    :param iface: A QGIS interface instance.
    :type iface: QgsInterface
    """
    from .epanet_geo_plugin import EpanetGeoPlugin
    return EpanetGeoPlugin(iface)
