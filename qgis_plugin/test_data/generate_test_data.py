#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate test data for EPANET GeoTools plugin.
Creates synthetic DEM and sample GIS files for testing.
No external dependencies required (pure Python).
"""

import os
import json
import math
import random

# Output directory
OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))


def generate_dem_asc(filename, xmin, ymin, xmax, ymax, resolution=1.0,
                      base_elev=700, slope_x=0.5, slope_y=-0.3):
    """
    Generate a synthetic DEM as an ESRI ASCII Grid file.
    Pure Python implementation - no numpy required.
    """
    # Calculate dimensions
    ncols = int((xmax - xmin) / resolution) + 1
    nrows = int((ymax - ymin) / resolution) + 1
    
    print(f"Generating DEM (ASCII): {ncols}x{nrows} cells, resolution {resolution}")
    
    # Set random seed for reproducibility
    random.seed(42)
    
    # Generate elevation data row by row
    terrain = []
    elev_min = float('inf')
    elev_max = float('-inf')
    
    for row in range(nrows):
        terrain_row = []
        # Y coordinate (normalized 0-1, inverted because ASC is top-to-bottom)
        y_norm = 1.0 - (row / (nrows - 1)) if nrows > 1 else 0.5
        
        for col in range(ncols):
            # X coordinate (normalized 0-1)
            x_norm = col / (ncols - 1) if ncols > 1 else 0.5
            
            # Base terrain with slope
            elev = base_elev + slope_x * (x_norm * 100) + slope_y * (y_norm * 100)
            
            # Add gentle hills using sin waves
            elev += 20 * math.sin(x_norm * math.pi * 2) * math.sin(y_norm * math.pi * 2)
            elev += 10 * math.sin(x_norm * math.pi * 4) * math.cos(y_norm * math.pi * 3)
            
            # Add slight random noise
            elev += random.gauss(0, 2)
            
            # Ensure minimum elevation
            elev = max(elev, 600)
            
            terrain_row.append(elev)
            elev_min = min(elev_min, elev)
            elev_max = max(elev_max, elev)
        
        terrain.append(terrain_row)
    
    # Write ASC file
    with open(filename, 'w') as f:
        f.write(f"ncols         {ncols}\n")
        f.write(f"nrows         {nrows}\n")
        f.write(f"xllcorner     {xmin}\n")
        f.write(f"yllcorner     {ymin}\n")
        f.write(f"cellsize      {resolution}\n")
        f.write(f"NODATA_value  -9999\n")
        
        for row in terrain:
            row_str = ' '.join(f'{v:.2f}' for v in row)
            f.write(row_str + '\n')
    
    print(f"Created: {filename}")
    print(f"  Extent: ({xmin}, {ymin}) to ({xmax}, {ymax})")
    print(f"  Elevation range: {elev_min:.1f} to {elev_max:.1f}")
    
    return filename


def generate_net1_geojson(output_dir):
    """
    Generate GeoJSON for Net1.inp network.
    Creates both nodes and pipes as separate feature collections.
    """
    # Net1 node data (from the INP file)
    nodes = {
        '10': {'x': 20, 'y': 70, 'elev': 710, 'demand': 0, 'type': 'junction'},
        '11': {'x': 30, 'y': 70, 'elev': 710, 'demand': 150, 'type': 'junction'},
        '12': {'x': 50, 'y': 70, 'elev': 700, 'demand': 150, 'type': 'junction'},
        '13': {'x': 70, 'y': 70, 'elev': 695, 'demand': 100, 'type': 'junction'},
        '21': {'x': 30, 'y': 40, 'elev': 700, 'demand': 150, 'type': 'junction'},
        '22': {'x': 50, 'y': 40, 'elev': 695, 'demand': 200, 'type': 'junction'},
        '23': {'x': 70, 'y': 40, 'elev': 690, 'demand': 150, 'type': 'junction'},
        '31': {'x': 30, 'y': 10, 'elev': 700, 'demand': 100, 'type': 'junction'},
        '32': {'x': 50, 'y': 10, 'elev': 710, 'demand': 100, 'type': 'junction'},
        '9':  {'x': 10, 'y': 70, 'elev': 800, 'demand': 0, 'type': 'reservoir'},
        '2':  {'x': 50, 'y': 90, 'elev': 850, 'demand': 0, 'type': 'tank'},
    }
    
    # Net1 pipe data
    pipes = {
        '10':  {'n1': '10', 'n2': '11', 'length': 10530, 'diameter': 18, 'roughness': 100},
        '11':  {'n1': '11', 'n2': '12', 'length': 5280, 'diameter': 14, 'roughness': 100},
        '12':  {'n1': '12', 'n2': '13', 'length': 5280, 'diameter': 10, 'roughness': 100},
        '21':  {'n1': '21', 'n2': '22', 'length': 5280, 'diameter': 10, 'roughness': 100},
        '22':  {'n1': '22', 'n2': '23', 'length': 5280, 'diameter': 12, 'roughness': 100},
        '31':  {'n1': '31', 'n2': '32', 'length': 5280, 'diameter': 6, 'roughness': 100},
        '110': {'n1': '2',  'n2': '12', 'length': 200, 'diameter': 18, 'roughness': 100},
        '111': {'n1': '11', 'n2': '21', 'length': 5280, 'diameter': 10, 'roughness': 100},
        '112': {'n1': '12', 'n2': '22', 'length': 5280, 'diameter': 12, 'roughness': 100},
        '113': {'n1': '13', 'n2': '23', 'length': 5280, 'diameter': 8, 'roughness': 100},
        '121': {'n1': '21', 'n2': '31', 'length': 5280, 'diameter': 8, 'roughness': 100},
        '122': {'n1': '22', 'n2': '32', 'length': 5280, 'diameter': 6, 'roughness': 100},
    }
    
    # Create nodes GeoJSON
    node_features = []
    for node_id, data in nodes.items():
        feature = {
            'type': 'Feature',
            'properties': {
                'id': node_id,
                'elevation': data['elev'],
                'demand': data['demand'],
                'node_type': data['type']
            },
            'geometry': {
                'type': 'Point',
                'coordinates': [data['x'], data['y']]
            }
        }
        node_features.append(feature)
    
    nodes_geojson = {
        'type': 'FeatureCollection',
        'name': 'Net1_Nodes',
        'features': node_features
    }
    
    # Create pipes GeoJSON
    pipe_features = []
    for pipe_id, data in pipes.items():
        n1 = nodes[data['n1']]
        n2 = nodes[data['n2']]
        feature = {
            'type': 'Feature',
            'properties': {
                'id': pipe_id,
                'length': data['length'],
                'diameter': data['diameter'],
                'roughness': data['roughness'],
                'node1': data['n1'],
                'node2': data['n2']
            },
            'geometry': {
                'type': 'LineString',
                'coordinates': [[n1['x'], n1['y']], [n2['x'], n2['y']]]
            }
        }
        pipe_features.append(feature)
    
    pipes_geojson = {
        'type': 'FeatureCollection',
        'name': 'Net1_Pipes',
        'features': pipe_features
    }
    
    # Write files
    nodes_file = os.path.join(output_dir, 'net1_nodes.geojson')
    pipes_file = os.path.join(output_dir, 'net1_pipes.geojson')
    
    with open(nodes_file, 'w') as f:
        json.dump(nodes_geojson, f, indent=2)
    print(f"Created: {nodes_file}")
    
    with open(pipes_file, 'w') as f:
        json.dump(pipes_geojson, f, indent=2)
    print(f"Created: {pipes_file}")
    
    return nodes_file, pipes_file


def generate_qgis_project(output_dir, dem_file, nodes_file, pipes_file):
    """
    Generate a QGIS project file (.qgs) that loads all test data.
    """
    project_file = os.path.join(output_dir, 'net1_test_project.qgs')
    
    # Simplified QGIS 3 project XML
    qgs_content = f'''<!DOCTYPE qgis PUBLIC 'http://mrcc.com/qgis.dtd' 'SYSTEM'>
<qgis version="3.0" projectname="EPANET Net1 Test">
  <title>EPANET Net1 Test Project</title>
  <layer-tree-group>
    <layer-tree-layer id="dem" name="DEM" source="{os.path.basename(dem_file)}"/>
    <layer-tree-layer id="pipes" name="Pipes" source="{os.path.basename(pipes_file)}"/>
    <layer-tree-layer id="nodes" name="Nodes" source="{os.path.basename(nodes_file)}"/>
  </layer-tree-group>
</qgis>
'''
    
    with open(project_file, 'w') as f:
        f.write(qgs_content)
    
    print(f"Created: {project_file}")
    return project_file


def main():
    """Generate all test data."""
    print("=" * 60)
    print("EPANET GeoTools Test Data Generator")
    print("=" * 60)
    
    # Ensure output directory exists
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    
    # Net1 coordinate extent (with buffer)
    xmin, ymin = 0, 0
    xmax, ymax = 80, 100
    
    # Generate DEM (ASCII format - most compatible)
    print("\n[1/3] Generating synthetic DEM...")
    dem_file = os.path.join(OUTPUT_DIR, 'net1_dem.asc')
    generate_dem_asc(
        dem_file, xmin, ymin, xmax, ymax,
        resolution=1.0,
        base_elev=700,
        slope_x=0.2,
        slope_y=-0.15
    )
    
    # Generate GeoJSON
    print("\n[2/3] Generating GeoJSON files...")
    nodes_file, pipes_file = generate_net1_geojson(OUTPUT_DIR)
    
    # Generate QGIS project
    print("\n[3/3] Creating QGIS project file...")
    generate_qgis_project(OUTPUT_DIR, dem_file, nodes_file, pipes_file)
    
    # Summary
    print("\n" + "=" * 60)
    print("Test data generation complete!")
    print("=" * 60)
    print(f"\nGenerated files in: {OUTPUT_DIR}")
    print(f"  - net1_dem.asc (synthetic terrain)")
    print(f"  - net1_nodes.geojson (network junctions)")
    print(f"  - net1_pipes.geojson (network pipes)")
    print(f"  - net1_test_project.qgs (QGIS project)")
    print("\nTo use in QGIS:")
    print("  1. Open QGIS")
    print("  2. Drag and drop the .asc and .geojson files, OR")
    print("  3. Open the .qgs project file")
    print("  4. Use EPANET GeoTools to work with the data")


if __name__ == '__main__':
    main()
