# -*- coding: utf-8 -*-
"""
EPANET GeoTools - Main Plugin Class
"""

import os
from qgis.PyQt.QtCore import QSettings, QTranslator, QCoreApplication, Qt
from qgis.PyQt.QtGui import QIcon, QColor
from qgis.PyQt.QtWidgets import QAction, QToolBar, QMenu, QFileDialog, QMessageBox
from qgis.core import (
    QgsProject, QgsVectorLayer, QgsRasterLayer,
    QgsFeature, QgsGeometry, QgsPointXY, QgsField,
    QgsWkbTypes, QgsCoordinateReferenceSystem,
    QgsSymbol, QgsRendererRange, QgsGraduatedSymbolRenderer,
    QgsStyle, Qgis, QgsApplication
)
from qgis.gui import QgsMapToolEmitPoint

from .epanet_bindings import EpanetProject, EpanetGeo
from .dialogs import (
    ImportDialog, ExportDialog, DEMDialog, 
    SimulationDialog, SettingsDialog
)


class EpanetGeoPlugin:
    """QGIS Plugin Implementation."""

    def __init__(self, iface):
        """Constructor.
        
        :param iface: An interface instance that will be passed to this class
            which provides the hook by which you can manipulate the QGIS
            application at run time.
        :type iface: QgsInterface
        """
        self.iface = iface
        self.plugin_dir = os.path.dirname(__file__)
        
        # Initialize plugin state
        self.actions = []
        self.menu = '&EPANET GeoTools'
        self.toolbar = None
        
        # EPANET project and geo context
        self.project = None
        self.geo = None
        
        # Layer references
        self.junction_layer = None
        self.reservoir_layer = None
        self.tank_layer = None
        self.pipe_layer = None
        self.pump_layer = None
        self.valve_layer = None

    def initGui(self):
        """Create the menu entries and toolbar icons inside the QGIS GUI."""
        
        # Create toolbar
        self.toolbar = self.iface.addToolBar('EPANET GeoTools')
        self.toolbar.setObjectName('EpanetGeoToolbar')
        
        # Create actions using QGIS theme icons
        self._add_action(
            'mActionFileNew',
            'New EPANET Project',
            self.new_project,
            add_to_toolbar=True
        )
        
        self._add_action(
            'mActionFileOpen',
            'Open INP File...',
            self.open_inp,
            add_to_toolbar=True
        )
        
        self._add_action(
            'mActionFileSave',
            'Save INP File...',
            self.save_inp,
            add_to_toolbar=True
        )
        
        # Separator
        self.toolbar.addSeparator()
        
        self._add_action(
            'mActionAddOgrLayer',
            'Import from GIS...',
            self.import_gis,
            add_to_toolbar=True
        )
        
        self._add_action(
            'mActionSaveMapAsImage',
            'Export to GIS...',
            self.export_gis,
            add_to_toolbar=True
        )
        
        # Separator
        self.toolbar.addSeparator()
        
        self._add_action(
            'mActionAddRasterLayer',
            'Assign Elevations from DEM...',
            self.assign_elevations,
            add_to_toolbar=True
        )
        
        self._add_action(
            'mActionMeasure',
            'Calculate Pipe Lengths',
            self.calc_pipe_lengths,
            add_to_toolbar=True
        )
        
        # Separator
        self.toolbar.addSeparator()
        
        self._add_action(
            'mActionStart',
            'Run Simulation',
            self.run_simulation,
            add_to_toolbar=True
        )
        
        self._add_action(
            'mActionOpenTable',
            'View Results...',
            self.view_results,
            add_to_toolbar=True
        )
        
        # Separator
        self.toolbar.addSeparator()
        
        self._add_action(
            'mActionOptions',
            'Settings...',
            self.show_settings,
            add_to_toolbar=True
        )

    def _add_action(self, icon_name, text, callback, add_to_toolbar=True,
                    add_to_menu=True, parent=None):
        """Add a toolbar icon to the toolbar."""
        
        # Use QGIS theme icon
        icon = QgsApplication.getThemeIcon(icon_name)
        action = QAction(icon, text, parent or self.iface.mainWindow())
        action.triggered.connect(callback)
        
        if add_to_toolbar:
            self.toolbar.addAction(action)
        
        if add_to_menu:
            self.iface.addPluginToMenu(self.menu, action)
        
        self.actions.append(action)
        return action

    def unload(self):
        """Removes the plugin menu item and icon from QGIS GUI."""
        for action in self.actions:
            self.iface.removePluginMenu(self.menu, action)
            self.iface.removeToolBarIcon(action)
        
        if self.toolbar:
            del self.toolbar
        
        # Clean up EPANET resources
        if self.geo:
            self.geo.destroy()
        if self.project:
            self.project.close()

    # -------------------------------------------------------------------------
    # Project Management
    # -------------------------------------------------------------------------
    
    def new_project(self):
        """Create a new EPANET project."""
        if self.project:
            reply = QMessageBox.question(
                self.iface.mainWindow(),
                'New Project',
                'Close current project and create new one?',
                QMessageBox.Yes | QMessageBox.No
            )
            if reply == QMessageBox.No:
                return
            self.project.close()
        
        self.project = EpanetProject()
        self.project.create()
        
        self.geo = EpanetGeo()
        self.geo.create()
        self.geo.attach(self.project)
        
        # Set CRS from QGIS project
        crs = QgsProject.instance().crs()
        if crs.isValid():
            auth_id = crs.authid()
            if auth_id.startswith('EPSG:'):
                epsg = int(auth_id.split(':')[1])
                self.geo.set_crs_epsg(epsg)
        
        self._create_layers()
        self.iface.messageBar().pushMessage(
            "EPANET", "New project created", level=Qgis.Info
        )

    def open_inp(self):
        """Open an existing INP file."""
        filename, _ = QFileDialog.getOpenFileName(
            self.iface.mainWindow(),
            'Open EPANET Input File',
            '',
            'EPANET Files (*.inp);;All Files (*)'
        )
        
        if not filename:
            return
        
        if self.project:
            self.project.close()
        
        self.project = EpanetProject()
        err = self.project.open(filename)
        
        if err != 0:
            QMessageBox.critical(
                self.iface.mainWindow(),
                'Error',
                f'Failed to open file: {self.project.get_error(err)}'
            )
            return
        
        self.geo = EpanetGeo()
        self.geo.create()
        self.geo.attach(self.project)
        
        self._create_layers()
        self._load_network_to_layers()
        
        self.iface.messageBar().pushMessage(
            "EPANET", f"Opened: {os.path.basename(filename)}", level=Qgis.Info
        )

    def save_inp(self):
        """Save the current project to INP file."""
        if not self.project:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                'No project open'
            )
            return
        
        filename, _ = QFileDialog.getSaveFileName(
            self.iface.mainWindow(),
            'Save EPANET Input File',
            '',
            'EPANET Files (*.inp);;All Files (*)'
        )
        
        if filename:
            self.project.save(filename)
            self.iface.messageBar().pushMessage(
                "EPANET", f"Saved: {os.path.basename(filename)}", level=Qgis.Info
            )

    # -------------------------------------------------------------------------
    # GIS Import/Export
    # -------------------------------------------------------------------------
    
    def import_gis(self):
        """Import network from GIS files."""
        if not self.project:
            self.new_project()
        
        dialog = ImportDialog(self.iface.mainWindow(), self.geo)
        if dialog.exec():
            self._load_network_to_layers()
            self.iface.messageBar().pushMessage(
                "EPANET", "Import complete", level=Qgis.Info
            )

    def export_gis(self):
        """Export network to GIS files."""
        if not self.project:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                'No project open'
            )
            return
        
        dialog = ExportDialog(self.iface.mainWindow(), self.geo)
        dialog.exec()

    # -------------------------------------------------------------------------
    # Terrain/Elevation
    # -------------------------------------------------------------------------
    
    def assign_elevations(self):
        """Assign node elevations from DEM raster."""
        if not self.project:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                'No project open'
            )
            return
        
        dialog = DEMDialog(self.iface.mainWindow(), self.geo)
        if dialog.exec():
            self._load_network_to_layers()
            self.iface.messageBar().pushMessage(
                "EPANET", "Elevations assigned from DEM", level=Qgis.Info
            )

    def calc_pipe_lengths(self):
        """Calculate pipe lengths from geometry."""
        if not self.project:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                'No project open'
            )
            return
        
        err = self.geo.calc_pipe_lengths(update=True)
        if err == 0:
            self._load_network_to_layers()
            self.iface.messageBar().pushMessage(
                "EPANET", "Pipe lengths calculated", level=Qgis.Info
            )
        else:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                f'Error: {self.geo.get_error()}'
            )

    # -------------------------------------------------------------------------
    # Simulation
    # -------------------------------------------------------------------------
    
    def run_simulation(self):
        """Run hydraulic/water quality simulation."""
        if not self.project:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                'No project open'
            )
            return
        
        dialog = SimulationDialog(self.iface.mainWindow(), self.project)
        if dialog.exec():
            self._apply_result_styling()
            self.iface.messageBar().pushMessage(
                "EPANET", "Simulation complete", level=Qgis.Success
            )

    def view_results(self):
        """Open results viewer dialog."""
        if not self.project:
            QMessageBox.warning(
                self.iface.mainWindow(),
                'Warning',
                'No project open'
            )
            return
        
        # TODO: Implement results viewer
        QMessageBox.information(
            self.iface.mainWindow(),
            'Results',
            'Results viewer coming soon'
        )

    # -------------------------------------------------------------------------
    # Settings
    # -------------------------------------------------------------------------
    
    def show_settings(self):
        """Show plugin settings dialog."""
        dialog = SettingsDialog(self.iface.mainWindow())
        dialog.exec()

    # -------------------------------------------------------------------------
    # Layer Management
    # -------------------------------------------------------------------------
    
    def _create_layers(self):
        """Create memory layers for network elements."""
        crs = QgsProject.instance().crs()
        crs_str = crs.authid() if crs.isValid() else 'EPSG:4326'
        
        # Junctions layer
        self.junction_layer = QgsVectorLayer(
            f'Point?crs={crs_str}', 'Junctions', 'memory'
        )
        self._add_node_fields(self.junction_layer)
        
        # Reservoirs layer
        self.reservoir_layer = QgsVectorLayer(
            f'Point?crs={crs_str}', 'Reservoirs', 'memory'
        )
        self._add_node_fields(self.reservoir_layer)
        
        # Tanks layer
        self.tank_layer = QgsVectorLayer(
            f'Point?crs={crs_str}', 'Tanks', 'memory'
        )
        self._add_node_fields(self.tank_layer)
        self._add_tank_fields(self.tank_layer)
        
        # Pipes layer
        self.pipe_layer = QgsVectorLayer(
            f'LineString?crs={crs_str}', 'Pipes', 'memory'
        )
        self._add_link_fields(self.pipe_layer)
        self._add_pipe_fields(self.pipe_layer)
        
        # Pumps layer
        self.pump_layer = QgsVectorLayer(
            f'LineString?crs={crs_str}', 'Pumps', 'memory'
        )
        self._add_link_fields(self.pump_layer)
        
        # Valves layer
        self.valve_layer = QgsVectorLayer(
            f'LineString?crs={crs_str}', 'Valves', 'memory'
        )
        self._add_link_fields(self.valve_layer)
        
        # Add layers to project
        QgsProject.instance().addMapLayers([
            self.junction_layer,
            self.reservoir_layer,
            self.tank_layer,
            self.pipe_layer,
            self.pump_layer,
            self.valve_layer
        ])

    def _add_node_fields(self, layer):
        """Add common node fields to a layer."""
        from qgis.PyQt.QtCore import QVariant
        provider = layer.dataProvider()
        provider.addAttributes([
            QgsField('id', QVariant.String),
            QgsField('elevation', QVariant.Double),
            QgsField('demand', QVariant.Double),
            QgsField('head', QVariant.Double),
            QgsField('pressure', QVariant.Double),
            QgsField('quality', QVariant.Double),
        ])
        layer.updateFields()

    def _add_tank_fields(self, layer):
        """Add tank-specific fields."""
        from qgis.PyQt.QtCore import QVariant
        provider = layer.dataProvider()
        provider.addAttributes([
            QgsField('init_level', QVariant.Double),
            QgsField('min_level', QVariant.Double),
            QgsField('max_level', QVariant.Double),
            QgsField('diameter', QVariant.Double),
        ])
        layer.updateFields()

    def _add_link_fields(self, layer):
        """Add common link fields to a layer."""
        from qgis.PyQt.QtCore import QVariant
        provider = layer.dataProvider()
        provider.addAttributes([
            QgsField('id', QVariant.String),
            QgsField('node1', QVariant.String),
            QgsField('node2', QVariant.String),
            QgsField('length', QVariant.Double),
            QgsField('flow', QVariant.Double),
            QgsField('velocity', QVariant.Double),
            QgsField('headloss', QVariant.Double),
            QgsField('status', QVariant.String),
        ])
        layer.updateFields()

    def _add_pipe_fields(self, layer):
        """Add pipe-specific fields."""
        from qgis.PyQt.QtCore import QVariant
        provider = layer.dataProvider()
        provider.addAttributes([
            QgsField('diameter', QVariant.Double),
            QgsField('roughness', QVariant.Double),
        ])
        layer.updateFields()

    def _load_network_to_layers(self):
        """Load network elements from EPANET project to QGIS layers."""
        if not self.project:
            return
        
        # Clear existing features
        for layer in [self.junction_layer, self.reservoir_layer, 
                      self.tank_layer, self.pipe_layer,
                      self.pump_layer, self.valve_layer]:
            if layer:
                layer.dataProvider().truncate()
        
        # Load junctions
        self._load_nodes(self.junction_layer, 0)  # EN_JUNCTION
        self._load_nodes(self.reservoir_layer, 1)  # EN_RESERVOIR
        self._load_nodes(self.tank_layer, 2)  # EN_TANK
        
        # Load links
        self._load_links(self.pipe_layer, 1)  # EN_PIPE
        self._load_links(self.pump_layer, 2)  # EN_PUMP
        self._load_links(self.valve_layer, 3)  # EN_VALVE (PRV)
        
        # Refresh layers
        for layer in [self.junction_layer, self.reservoir_layer,
                      self.tank_layer, self.pipe_layer,
                      self.pump_layer, self.valve_layer]:
            if layer:
                layer.triggerRepaint()

    def _load_nodes(self, layer, node_type):
        """Load nodes of a specific type to a layer."""
        if not layer:
            return
        
        features = []
        node_count = self.project.get_count(0)  # EN_NODECOUNT
        
        for i in range(1, node_count + 1):
            ntype = self.project.get_node_type(i)
            if ntype != node_type:
                continue
            
            node_id = self.project.get_node_id(i)
            x, y = self.project.get_coord(i)
            elev = self.project.get_node_value(i, 0)  # EN_ELEVATION
            demand = self.project.get_node_value(i, 1)  # EN_BASEDEMAND
            
            feat = QgsFeature(layer.fields())
            feat.setGeometry(QgsGeometry.fromPointXY(QgsPointXY(x, y)))
            feat.setAttributes([node_id, elev, demand, 0, 0, 0])
            features.append(feat)
        
        layer.dataProvider().addFeatures(features)

    def _load_links(self, layer, link_type):
        """Load links of a specific type to a layer."""
        if not layer:
            return
        
        features = []
        link_count = self.project.get_count(2)  # EN_LINKCOUNT
        
        for i in range(1, link_count + 1):
            ltype = self.project.get_link_type(i)
            
            # Map link types (pipe=1, pump=2, valves=3-9)
            if link_type == 1 and ltype not in [1]:  # Pipe
                continue
            if link_type == 2 and ltype != 2:  # Pump
                continue
            if link_type == 3 and ltype < 3:  # Valves
                continue
            
            link_id = self.project.get_link_id(i)
            n1, n2 = self.project.get_link_nodes(i)
            x1, y1 = self.project.get_coord(n1)
            x2, y2 = self.project.get_coord(n2)
            
            length = self.project.get_link_value(i, 1)  # EN_LENGTH
            
            # Build line with vertices
            points = [QgsPointXY(x1, y1)]
            vertex_count = self.project.get_vertex_count(i)
            for v in range(1, vertex_count + 1):
                vx, vy = self.project.get_vertex(i, v)
                points.append(QgsPointXY(vx, vy))
            points.append(QgsPointXY(x2, y2))
            
            feat = QgsFeature(layer.fields())
            feat.setGeometry(QgsGeometry.fromPolylineXY(points))
            
            if link_type == 1:  # Pipe has extra fields
                diam = self.project.get_link_value(i, 0)  # EN_DIAMETER
                rough = self.project.get_link_value(i, 2)  # EN_ROUGHNESS
                feat.setAttributes([
                    link_id, str(n1), str(n2), length, 0, 0, 0, 'OPEN',
                    diam, rough
                ])
            else:
                feat.setAttributes([
                    link_id, str(n1), str(n2), length, 0, 0, 0, 'OPEN'
                ])
            
            features.append(feat)
        
        layer.dataProvider().addFeatures(features)

    def _apply_result_styling(self):
        """Apply graduated styling based on simulation results."""
        # Apply pressure styling to junctions
        if self.junction_layer:
            self._apply_graduated_style(
                self.junction_layer, 'pressure',
                ['#2166ac', '#67a9cf', '#d1e5f0', '#fddbc7', '#ef8a62', '#b2182b'],
                [0, 20, 40, 60, 80, 100]
            )
        
        # Apply flow styling to pipes
        if self.pipe_layer:
            self._apply_graduated_style(
                self.pipe_layer, 'flow',
                ['#ffffb2', '#fecc5c', '#fd8d3c', '#f03b20', '#bd0026'],
                [0, 100, 500, 1000, 5000]
            )

    def _apply_graduated_style(self, layer, field, colors, breaks):
        """Apply graduated symbology to a layer."""
        ranges = []
        for i in range(len(breaks) - 1):
            symbol = QgsSymbol.defaultSymbol(layer.geometryType())
            symbol.setColor(QColor(colors[i]))
            rng = QgsRendererRange(
                breaks[i], breaks[i + 1],
                symbol,
                f'{breaks[i]} - {breaks[i + 1]}'
            )
            ranges.append(rng)
        
        renderer = QgsGraduatedSymbolRenderer(field, ranges)
        layer.setRenderer(renderer)
        layer.triggerRepaint()
