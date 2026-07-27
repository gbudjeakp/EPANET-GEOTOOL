# -*- coding: utf-8 -*-
"""
Dialog windows for EPANET GeoTools plugin
"""

import os
from qgis.PyQt.QtCore import Qt
from qgis.PyQt.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
    QLabel, QLineEdit, QPushButton, QComboBox, QCheckBox,
    QFileDialog, QGroupBox, QProgressBar, QDialogButtonBox,
    QSpinBox, QDoubleSpinBox, QTabWidget, QWidget, QMessageBox
)
from qgis.core import QgsProject, QgsMapLayerProxyModel
from qgis.gui import QgsFileWidget, QgsMapLayerComboBox


class ImportDialog(QDialog):
    """Dialog for importing GIS data."""
    
    def __init__(self, parent, geo):
        super().__init__(parent)
        self.geo = geo
        self.setWindowTitle('Import from GIS')
        self.setMinimumWidth(500)
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        
        # File selection
        file_group = QGroupBox('Source File')
        file_layout = QFormLayout(file_group)
        
        self.file_widget = QgsFileWidget()
        self.file_widget.setStorageMode(QgsFileWidget.GetFile)
        self.file_widget.setFilter('GeoJSON (*.geojson *.json);;Shapefiles (*.shp);;GeoPackage (*.gpkg);;All Files (*.*)')
        file_layout.addRow('File:', self.file_widget)
        
        layout.addWidget(file_group)
        
        # Layer type selection
        type_group = QGroupBox('Import Options')
        type_layout = QFormLayout(type_group)
        
        self.layer_type = QComboBox()
        self.layer_type.addItems([
            'Junctions (points)',
            'Reservoirs (points)',
            'Tanks (points)',
            'Pipes (lines)',
            'All (auto-detect)'
        ])
        self.layer_type.setCurrentIndex(4)
        type_layout.addRow('Layer Type:', self.layer_type)
        
        self.auto_attrs = QCheckBox('Auto-detect attribute mapping')
        self.auto_attrs.setChecked(True)
        type_layout.addRow(self.auto_attrs)
        
        layout.addWidget(type_group)
        
        # Attribute mapping (optional)
        self.attr_group = QGroupBox('Attribute Mapping')
        self.attr_group.setCheckable(True)
        self.attr_group.setChecked(False)
        attr_layout = QFormLayout(self.attr_group)
        
        self.id_field = QLineEdit()
        self.id_field.setPlaceholderText('ID, NAME, OBJECTID...')
        attr_layout.addRow('ID Field:', self.id_field)
        
        self.elev_field = QLineEdit()
        self.elev_field.setPlaceholderText('ELEVATION, ELEV, Z...')
        attr_layout.addRow('Elevation Field:', self.elev_field)
        
        self.demand_field = QLineEdit()
        self.demand_field.setPlaceholderText('DEMAND, BASE_DEMAND...')
        attr_layout.addRow('Demand Field:', self.demand_field)
        
        self.diameter_field = QLineEdit()
        self.diameter_field.setPlaceholderText('DIAMETER, DIAM, DIA...')
        attr_layout.addRow('Diameter Field:', self.diameter_field)
        
        self.roughness_field = QLineEdit()
        self.roughness_field.setPlaceholderText('ROUGHNESS, ROUGH, C...')
        attr_layout.addRow('Roughness Field:', self.roughness_field)
        
        layout.addWidget(self.attr_group)
        
        # Progress
        self.progress = QProgressBar()
        self.progress.setVisible(False)
        layout.addWidget(self.progress)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._do_import)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        
        # Connect signals
        self.auto_attrs.toggled.connect(
            lambda checked: self.attr_group.setEnabled(not checked)
        )
    
    def _do_import(self):
        """Perform the import."""
        filepath = self.file_widget.filePath()
        if not filepath or not os.path.exists(filepath):
            QMessageBox.warning(self, 'Error', 'Please select a valid file')
            return
        
        self.progress.setVisible(True)
        self.progress.setValue(0)
        
        layer_type = self.layer_type.currentIndex()
        
        try:
            if layer_type == 0:  # Junctions
                err = self.geo.import_nodes_shp(filepath, 0)
            elif layer_type == 1:  # Reservoirs
                err = self.geo.import_nodes_shp(filepath, 1)
            elif layer_type == 2:  # Tanks
                err = self.geo.import_nodes_shp(filepath, 2)
            elif layer_type == 3:  # Pipes
                err = self.geo.import_pipes_shp(filepath)
            else:  # Auto-detect
                if filepath.endswith('.geojson') or filepath.endswith('.json'):
                    err = self.geo.import_geojson(filepath)
                else:
                    # Try to detect from geometry
                    err = self.geo.import_pipes_shp(filepath)
                    if err != 0:
                        err = self.geo.import_nodes_shp(filepath, 0)
            
            self.progress.setValue(100)
            
            if err == 0:
                self.accept()
            else:
                QMessageBox.warning(
                    self, 'Import Error',
                    f'Import failed: {self.geo.get_error()}'
                )
        except Exception as e:
            QMessageBox.critical(self, 'Error', str(e))
        
        self.progress.setVisible(False)


class ExportDialog(QDialog):
    """Dialog for exporting to GIS formats."""
    
    def __init__(self, parent, geo):
        super().__init__(parent)
        self.geo = geo
        self.setWindowTitle('Export to GIS')
        self.setMinimumWidth(500)
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        
        # Format selection
        format_group = QGroupBox('Output Format')
        format_layout = QFormLayout(format_group)
        
        self.format_combo = QComboBox()
        self.format_combo.addItems([
            'Shapefile (.shp)',
            'GeoJSON (.geojson)',
            'GeoPackage (.gpkg)'
        ])
        format_layout.addRow('Format:', self.format_combo)
        
        self.output_dir = QgsFileWidget()
        self.output_dir.setStorageMode(QgsFileWidget.GetDirectory)
        format_layout.addRow('Output Directory:', self.output_dir)
        
        self.base_name = QLineEdit('epanet_network')
        format_layout.addRow('Base Filename:', self.base_name)
        
        layout.addWidget(format_group)
        
        # Export options
        options_group = QGroupBox('Export Options')
        options_layout = QVBoxLayout(options_group)
        
        self.export_junctions = QCheckBox('Junctions')
        self.export_junctions.setChecked(True)
        options_layout.addWidget(self.export_junctions)
        
        self.export_reservoirs = QCheckBox('Reservoirs')
        self.export_reservoirs.setChecked(True)
        options_layout.addWidget(self.export_reservoirs)
        
        self.export_tanks = QCheckBox('Tanks')
        self.export_tanks.setChecked(True)
        options_layout.addWidget(self.export_tanks)
        
        self.export_pipes = QCheckBox('Pipes')
        self.export_pipes.setChecked(True)
        options_layout.addWidget(self.export_pipes)
        
        self.export_pumps = QCheckBox('Pumps')
        self.export_pumps.setChecked(True)
        options_layout.addWidget(self.export_pumps)
        
        self.export_valves = QCheckBox('Valves')
        self.export_valves.setChecked(True)
        options_layout.addWidget(self.export_valves)
        
        self.include_results = QCheckBox('Include simulation results')
        self.include_results.setChecked(False)
        options_layout.addWidget(self.include_results)
        
        layout.addWidget(options_group)
        
        # Progress
        self.progress = QProgressBar()
        self.progress.setVisible(False)
        layout.addWidget(self.progress)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._do_export)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
    
    def _do_export(self):
        """Perform the export."""
        output_dir = self.output_dir.filePath()
        if not output_dir:
            QMessageBox.warning(self, 'Error', 'Please select output directory')
            return
        
        base_name = self.base_name.text() or 'epanet_network'
        fmt = self.format_combo.currentIndex()
        include_results = self.include_results.isChecked()
        
        self.progress.setVisible(True)
        self.progress.setValue(0)
        
        try:
            if fmt == 0:  # Shapefile
                ext = '.shp'
                if self.export_junctions.isChecked():
                    self.geo.export_nodes_shp(
                        os.path.join(output_dir, f'{base_name}_junctions{ext}'),
                        0, include_results
                    )
                if self.export_reservoirs.isChecked():
                    self.geo.export_nodes_shp(
                        os.path.join(output_dir, f'{base_name}_reservoirs{ext}'),
                        1, include_results
                    )
                if self.export_tanks.isChecked():
                    self.geo.export_nodes_shp(
                        os.path.join(output_dir, f'{base_name}_tanks{ext}'),
                        2, include_results
                    )
                if self.export_pipes.isChecked():
                    self.geo.export_links_shp(
                        os.path.join(output_dir, f'{base_name}_pipes{ext}'),
                        1, include_results
                    )
                    
            elif fmt == 1:  # GeoJSON
                filepath = os.path.join(output_dir, f'{base_name}.geojson')
                mode = 1 if include_results else 0
                self.geo.export_geojson(filepath, mode)
                
            elif fmt == 2:  # GeoPackage
                filepath = os.path.join(output_dir, f'{base_name}.gpkg')
                # TODO: Implement GeoPackage export
                QMessageBox.information(
                    self, 'Info',
                    'GeoPackage export coming soon. Use Shapefile or GeoJSON.'
                )
            
            self.progress.setValue(100)
            QMessageBox.information(self, 'Success', 'Export complete!')
            self.accept()
            
        except Exception as e:
            QMessageBox.critical(self, 'Error', str(e))
        
        self.progress.setVisible(False)


class DEMDialog(QDialog):
    """Dialog for DEM elevation assignment."""
    
    def __init__(self, parent, geo):
        super().__init__(parent)
        self.geo = geo
        self.setWindowTitle('Assign Elevations from DEM')
        self.setMinimumWidth(450)
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        
        # DEM selection
        dem_group = QGroupBox('DEM Source')
        dem_layout = QFormLayout(dem_group)
        
        # Option 1: From file
        self.dem_file = QgsFileWidget()
        self.dem_file.setFilter('Raster Files (*.tif *.tiff *.dem *.asc);;All Files (*)')
        dem_layout.addRow('DEM File:', self.dem_file)
        
        # Option 2: From loaded raster layer
        self.dem_layer = QgsMapLayerComboBox()
        self.dem_layer.setFilters(QgsMapLayerProxyModel.RasterLayer)
        dem_layout.addRow('Or Raster Layer:', self.dem_layer)
        
        layout.addWidget(dem_group)
        
        # Interpolation method
        method_group = QGroupBox('Interpolation Method')
        method_layout = QFormLayout(method_group)
        
        self.method_combo = QComboBox()
        self.method_combo.addItems([
            'Nearest Neighbor (fast)',
            'Bilinear (recommended)',
            'Cubic (smooth)'
        ])
        self.method_combo.setCurrentIndex(1)
        method_layout.addRow('Method:', self.method_combo)
        
        layout.addWidget(method_group)
        
        # Options
        options_group = QGroupBox('Options')
        options_layout = QVBoxLayout(options_group)
        
        self.overwrite = QCheckBox('Overwrite existing elevations')
        self.overwrite.setChecked(True)
        options_layout.addWidget(self.overwrite)
        
        self.skip_missing = QCheckBox('Skip nodes outside DEM extent')
        self.skip_missing.setChecked(True)
        options_layout.addWidget(self.skip_missing)
        
        layout.addWidget(options_group)
        
        # Progress
        self.progress = QProgressBar()
        self.progress.setVisible(False)
        layout.addWidget(self.progress)
        
        # Status
        self.status_label = QLabel('')
        layout.addWidget(self.status_label)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self._do_assign)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
    
    def _do_assign(self):
        """Assign elevations from DEM."""
        # Get DEM source
        dem_path = self.dem_file.filePath()
        if not dem_path:
            layer = self.dem_layer.currentLayer()
            if layer:
                dem_path = layer.source()
        
        if not dem_path:
            QMessageBox.warning(self, 'Error', 'Please select a DEM source')
            return
        
        self.progress.setVisible(True)
        self.progress.setValue(0)
        self.status_label.setText('Opening DEM...')
        
        try:
            err = self.geo.open_dem(dem_path)
            if err != 0:
                QMessageBox.warning(
                    self, 'Error',
                    f'Failed to open DEM: {self.geo.get_error()}'
                )
                return
            
            self.progress.setValue(50)
            self.status_label.setText('Assigning elevations...')
            
            method = self.method_combo.currentIndex()
            err = self.geo.assign_elevations(method)
            
            self.geo.close_dem()
            
            self.progress.setValue(100)
            
            if err == 0:
                self.status_label.setText('Done!')
                self.accept()
            else:
                QMessageBox.warning(
                    self, 'Warning',
                    f'Some nodes may be outside DEM: {self.geo.get_error()}'
                )
                self.accept()
                
        except Exception as e:
            QMessageBox.critical(self, 'Error', str(e))
        
        self.progress.setVisible(False)


class SimulationDialog(QDialog):
    """Dialog for running simulations."""
    
    def __init__(self, parent, project):
        super().__init__(parent)
        self.project = project
        self.setWindowTitle('Run Simulation')
        self.setMinimumWidth(400)
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        
        # Simulation type
        type_group = QGroupBox('Simulation Type')
        type_layout = QVBoxLayout(type_group)
        
        self.hydraulic_only = QCheckBox('Hydraulic Analysis')
        self.hydraulic_only.setChecked(True)
        type_layout.addWidget(self.hydraulic_only)
        
        self.water_quality = QCheckBox('Water Quality Analysis')
        self.water_quality.setChecked(False)
        type_layout.addWidget(self.water_quality)
        
        layout.addWidget(type_group)
        
        # Duration (for extended period)
        duration_group = QGroupBox('Simulation Duration')
        duration_layout = QFormLayout(duration_group)
        
        self.duration = QSpinBox()
        self.duration.setRange(0, 8760)
        self.duration.setValue(0)
        self.duration.setSuffix(' hours')
        duration_layout.addRow('Duration:', self.duration)
        
        self.timestep = QSpinBox()
        self.timestep.setRange(1, 60)
        self.timestep.setValue(1)
        self.timestep.setSuffix(' hours')
        duration_layout.addRow('Hydraulic Timestep:', self.timestep)
        
        layout.addWidget(duration_group)
        
        # Progress
        self.progress = QProgressBar()
        self.progress.setVisible(False)
        layout.addWidget(self.progress)
        
        self.status_label = QLabel('')
        layout.addWidget(self.status_label)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.button(QDialogButtonBox.StandardButton.Ok).setText('Run')
        buttons.accepted.connect(self._run_simulation)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
    
    def _run_simulation(self):
        """Run the simulation."""
        self.progress.setVisible(True)
        self.progress.setValue(0)
        self.status_label.setText('Running hydraulic analysis...')
        
        try:
            err = self.project.run_hydraulics()
            
            self.progress.setValue(100)
            
            if err == 0:
                self.status_label.setText('Simulation complete!')
                self.accept()
            else:
                QMessageBox.warning(
                    self, 'Simulation Error',
                    f'Error: {self.project.get_error(err)}'
                )
                
        except Exception as e:
            QMessageBox.critical(self, 'Error', str(e))
        
        self.progress.setVisible(False)


class SettingsDialog(QDialog):
    """Plugin settings dialog."""
    
    def __init__(self, parent):
        super().__init__(parent)
        self.setWindowTitle('EPANET GeoTools Settings')
        self.setMinimumWidth(450)
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        
        tabs = QTabWidget()
        
        # General tab
        general_tab = QWidget()
        general_layout = QFormLayout(general_tab)
        
        self.default_crs = QLineEdit('EPSG:4326')
        general_layout.addRow('Default CRS:', self.default_crs)
        
        self.units = QComboBox()
        self.units.addItems(['US Customary', 'SI Metric'])
        general_layout.addRow('Units:', self.units)
        
        tabs.addTab(general_tab, 'General')
        
        # Libraries tab
        lib_tab = QWidget()
        lib_layout = QFormLayout(lib_tab)
        
        self.epanet_lib = QgsFileWidget()
        self.epanet_lib.setFilter('Libraries (*.dylib *.so *.dll)')
        lib_layout.addRow('EPANET Library:', self.epanet_lib)
        
        self.geo_lib = QgsFileWidget()
        self.geo_lib.setFilter('Libraries (*.dylib *.so *.dll)')
        lib_layout.addRow('epanet-geo Library:', self.geo_lib)
        
        tabs.addTab(lib_tab, 'Libraries')
        
        # Display tab
        display_tab = QWidget()
        display_layout = QFormLayout(display_tab)
        
        self.junction_size = QDoubleSpinBox()
        self.junction_size.setRange(1, 20)
        self.junction_size.setValue(5)
        display_layout.addRow('Junction Size:', self.junction_size)
        
        self.pipe_width = QDoubleSpinBox()
        self.pipe_width.setRange(0.5, 10)
        self.pipe_width.setValue(2)
        display_layout.addRow('Pipe Width:', self.pipe_width)
        
        tabs.addTab(display_tab, 'Display')
        
        layout.addWidget(tabs)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
