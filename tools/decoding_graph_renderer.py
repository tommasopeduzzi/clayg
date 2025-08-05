#!/usr/bin/env python3
"""
3D Graph Visualizer with Performance Improvements, Error Overlay, Cluster Growth,
Edge/Edge Label Visibility Options, and Single Layer View with Always-Visible Layer Controls

Usage:
    python graph_visualizer_3d_perf.py <graph_file> <errors_file> [--clusters clusters_directory]

File formats:
---------------
Graph file:
  Each line contains an edge in the format:
      node1, node2, edge
  Each node (and the edge label) is encoded as "type-round-id" where:
      For nodes:
         "0" -> Ancilla
         "1" -> Virtual
      For edges:
         "0" -> Measurement
         "1" -> Normal

Errors file:
  Each line is an edge id (encoded as above) that should be highlighted as an error.

Cluster files (optional):
  If the argument --clusters is provided, the directory should contain files named
  like "cluster_0.txt", "cluster_1.txt", etc. Each such file lists only the edges that are part of a cluster.
  Each line in a cluster file has the format:
      {type}-{round}-{id}, {clusterid}

3D Node Layout Logic:
----------------------
  rounds = max([node_round for each node]) + 1
  distance = sqrt( count( edge where both endpoints have round==0 ) )
  nodes_per_layer = ceil(distance / 2)

  For each node (decoded as (type, round, id)):
    - Color: "red" if type == "Ancilla", otherwise "blue"
    - If type == "Virtual":
          if id < nodes_per_layer:
              x = id - 0.5
              y = 0
          else:
              x = id % nodes_per_layer
              y = distance
          z = (rounds - 1) / 2
    - Otherwise (Ancilla):
          x = (id % nodes_per_layer) - ((id // nodes_per_layer) % 2) * 0.5
          y = (id // nodes_per_layer) + 1
          z = round
    Finally, multiply x and y by 3.

Additional Option:
  A "Single Layer" mode will hide all Ancilla nodes not in a selected layer
  (but Virtual nodes are always rendered). In this mode the view’s z‑limits are adjusted
  so that the displayed layer appears at its actual height. Also, Virtual nodes are forced
  to be rendered at the selected layer's height.

Performance improvements include:
  - Caching computed layout in dictionaries and using NumPy arrays.
  - Grouping drawing into a 3D scatter and Line3DCollections.
  - Debouncing rapid update calls.
  - Manually setting axis limits to avoid autoscaling issues.
  - Overriding node color alpha uniformly.
  - Separate toggles for edge and edge label visibility.
  - When "Edges" is deselected, error edges remain visible if "Errors" is enabled.
  - A concise layer control UI (current layer label with plus and minus buttons) that is always visible.
  - A Reset View button that sets the camera to a top-down view.
"""

import argparse
import math
import os
import re
import sys
import time

import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
from matplotlib.colors import to_rgba
from matplotlib.widgets import CheckButtons, Slider, Button
from mpl_toolkits.mplot3d.art3d import Line3DCollection


def debounce(wait):
    def decorator(fn):
        last_call = [0]

        def debounced(*args, **kwargs):
            now = time.time()
            if now - last_call[0] > wait:
                last_call[0] = now
                return fn(*args, **kwargs)

        return debounced

    return decorator


class GraphVisualizer3D:
    def __init__(self, graph_file, errors_file, clusters_dir=None, corrections_file=None):
        self.graph_file = graph_file
        self.errors_file = errors_file
        self.clusters_dir = clusters_dir
        self.corrections_file = corrections_file

        # Data structures for the graph.
        self.G = nx.MultiGraph()
        self.error_edges = set()
        self.correction_edges = set()
        self.clusters = {}
        self.cluster_steps = []
        self.current_cluster_step = None

        # Toggle flags.
        self.show_nodes = True
        self.show_node_labels = False
        self.show_edges = False
        self.show_edge_labels = False
        self.show_errors = True
        self.show_corrections = True
        self.show_clusters = True if clusters_dir else False
        self.single_layer_enabled = False
        self.selected_layer = 0

        # Cached computed layout: node -> (x,y,z) and node colors.
        self.pos = {}
        self.node_colors = {}
        self.node_list = []

        # Artist handles.
        self.node_scatter = None
        self.normal_edge_collection = None
        self.error_edge_collection = None
        self.correction_edge_collection = None
        self.cluster_edge_collection = None
        self.node_texts = []
        self.edge_texts = []  # For edge labels.

        # For debouncing updates.
        self._draw_timer = None

        # Create figure and 3D axis.
        self.fig = plt.figure(figsize=(8, 6))
        self.ax = self.fig.add_subplot(111, projection='3d')
        plt.subplots_adjust(left=0.3, bottom=0.15)

        # Load files, compute layout, and create artists.
        self.load_graph()
        self.load_errors()
        self.load_corrections()
        if self.clusters_dir:
            self.load_clusters()
        self.compute_layout()
        self.create_artists()
        self.draw_graph()

    def parse_node_str(self, node_str):
        parts = node_str.split('-')
        if len(parts) != 3:
            raise ValueError(f"Invalid node string format: {node_str}")
        type_code = parts[0]
        node_type = "Ancilla" if type_code == "0" else "Virtual" if type_code == "1" else type_code
        try:
            round_ = int(parts[1])
            id_ = int(parts[2])
        except ValueError:
            raise ValueError(f"Non-integer round or id in: {node_str}")
        return (node_type, round_, id_)

    def parse_edge_str(self, edge_str):
        parts = edge_str.split('-')
        if len(parts) != 3:
            raise ValueError(f"Invalid edge string format: {edge_str}")
        code = parts[0]
        etype = "Measurement" if code == "0" else "Normal" if code == "1" else code
        try:
            round_ = int(parts[1])
            id_ = int(parts[2])
        except ValueError:
            raise ValueError(f"Non-integer round or id in: {edge_str}")
        return (etype, round_, id_)

    def load_graph(self):
        try:
            with open(self.graph_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    parts = line.split(',')
                    if len(parts) != 3:
                        continue
                    node1 = parts[0].strip()
                    node2 = parts[1].strip()
                    edge_label = parts[2].strip()
                    self.G.add_node(node1)
                    self.G.add_node(node2)
                    self.G.add_edge(node1, node2, label=edge_label)
        except Exception as e:
            print(f"Error reading graph file: {e}")
            sys.exit(1)
        self.node_list = list(self.G.nodes())

    def load_errors(self):
        try:
            with open(self.errors_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line:
                        self.error_edges.add(line)
        except Exception as e:
            print(f"Error reading errors file: {e}")
            sys.exit(1)

    def load_corrections(self):
        if self.corrections_file is None:
            return
        try:
            with open(self.corrections_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line:
                        self.correction_edges.add(line)
        except Exception as e:
            print(f"Error reading corrections file: {e}")
            sys.exit(1)

    def load_clusters(self):
        try:
            files = [f for f in os.listdir(self.clusters_dir) if f.endswith('.txt')]
        except Exception as e:
            print(f"Error reading clusters directory: {e}")
            sys.exit(1)
        steps = []
        for filename in files:
            # regex of the form "clusters_step_{step}.txt"
            re_match = re.search(r'clusters_step_(\d+)\.txt', filename)
            step = int(re_match.group(1)) if re_match else None
            if step is not None and step >= 0:
                steps.append(step)
        steps.sort()
        self.cluster_steps = steps
        for step in steps:
            cluster_file = os.path.join(self.clusters_dir, f"clusters_step_{step}.txt")
            step_clusters = {}
            try:
                with open(cluster_file, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        parts = line.split(',')
                        if len(parts) != 2:
                            continue
                        edge_id = parts[0].strip()
                        cluster_id = parts[1].strip()
                        step_clusters[edge_id] = cluster_id
                self.clusters[step] = step_clusters
            except Exception as e:
                print(f"Error reading cluster file {cluster_file}: {e}")
        if steps:
            self.current_cluster_step = steps[0]

    def compute_layout(self):
        """Compute positions and colors for all nodes using the provided logic."""
        parsed_nodes = []
        for node in self.node_list:
            try:
                parsed_nodes.append(self.parse_node_str(node))
            except Exception as e:
                print(f"Skipping node {node}: {e}")
        if not parsed_nodes:
            print("No valid nodes found.")
            return
        max_round = max(r for (_, r, _) in parsed_nodes)
        rounds = max_round + 1
        self.total_layers = rounds
        # Count edges with both endpoints in round 0.
        virtual_edge_count = 0
        for u, v in self.G.edges():
            try:
                _, r1, _ = self.parse_node_str(u)
                _, r2, _ = self.parse_node_str(v)
                if r1 == 0 and r2 == 0:
                    virtual_edge_count += 1
            except Exception:
                continue
        distance = virtual_edge_count ** 0.5
        nodes_per_layer = math.floor(distance / 2) + 1  if distance > 0 else 1

        for node in self.node_list:
            try:
                t, r, i = self.parse_node_str(node)
            except Exception:
                continue
            color = "red" if t == "Ancilla" else "blue"
            self.node_colors[node] = color
            if t == "Virtual":
                if i == 0:
                    x = nodes_per_layer / 2 - 0.5
                    y = 0
                else:
                    x = nodes_per_layer / 2 - 0.5
                    y = distance - 0.5
                z = (rounds - 1) / 2
            else:
                x = (i % nodes_per_layer) - ((i // nodes_per_layer) % 2) * 0.5
                y = (i // nodes_per_layer) + 1
                z = r
            self.pos[node] = (x * 3, y * 3, z)

    def create_artists(self):
        """Create initial scatter and edge collections using dummy empty arrays."""
        self.node_scatter = self.ax.scatter([], [], [], s=100, alpha=0.8)
        dummy_seg = np.empty((0, 2, 3))
        self.normal_edge_collection = Line3DCollection(dummy_seg, colors='black', linewidths=1)
        self.ax.add_collection3d(self.normal_edge_collection)
        self.error_edge_collection = Line3DCollection(dummy_seg, colors='red', linewidths=2.5)
        self.ax.add_collection3d(self.error_edge_collection)
        self.correction_edge_collection = Line3DCollection(dummy_seg, colors='green', linewidths=1.5)
        self.ax.add_collection3d(self.correction_edge_collection)
        self.cluster_edge_collection = Line3DCollection(dummy_seg, colors='gray', linewidths=3, linestyles='dashed')
        self.ax.add_collection3d(self.cluster_edge_collection)
        self.node_texts = []
        self.edge_texts = []

    def schedule_draw(self):
        """Debounce redraws by scheduling one after a short delay."""
        if self._draw_timer is not None:
            self._draw_timer.stop()
        self._draw_timer = self.fig.canvas.new_timer(interval=50)
        self._draw_timer.add_callback(self.draw_graph)
        self._draw_timer.start()

    def reset_view(self):
        """Reset the camera view to a top-down perspective."""
        self.ax.view_init(elev=90, azim=-90)
        self.fig.canvas.draw_idle()

    def draw_graph(self):
        """
        Update node and edge positions using cached collections.
        In single-layer mode, Virtual nodes are drawn at the selected layer's height.
        All node colors are converted to RGBA with a fixed alpha.
        Edge visibility and edge label visibility are handled as follows:
          - Normal edges are shown only if "Edges" is enabled.
          - Error edges are drawn if "Errors" is enabled.
          - Correction edges are drawn if "Corrections" is enabled.
          - Edge labels are drawn if "Edge Labels" is enabled (independent of the "Edges" toggle).
        """
        # Determine visible nodes.
        if self.single_layer_enabled:
            visible_nodes = []
            for node in self.node_list:
                try:
                    t, r, _ = self.parse_node_str(node)
                except Exception:
                    continue
                if t == "Virtual" or (t == "Ancilla" and r == self.selected_layer):
                    visible_nodes.append(node)
        else:
            visible_nodes = list(self.node_list)

        # Build local dictionary of positions for drawing.
        # In single-layer mode, force Virtual nodes to have z equal to selected_layer.
        draw_pos = {}
        for node in visible_nodes:
            if node in self.pos:
                x, y, z = self.pos[node]
                try:
                    t, r, _ = self.parse_node_str(node)
                except Exception:
                    t = None
                if self.single_layer_enabled and t == "Virtual":
                    z = self.selected_layer
                draw_pos[node] = (x, y, z)

        # Update node scatter.
        xs, ys, zs, colors = [], [], [], []
        if self.show_nodes:
            for node in visible_nodes:
                if node in draw_pos:
                    x, y, z = draw_pos[node]
                    xs.append(x)
                    ys.append(y)
                    zs.append(z)
                    colors.append(to_rgba(self.node_colors[node], alpha=0.8))
        if self.node_scatter is not None:
            self.node_scatter._offsets3d = (np.array(xs), np.array(ys), np.array(zs))
            self.node_scatter.set_color(colors)
            self.node_scatter.set_visible(self.show_nodes)
        else:
            self.node_scatter = self.ax.scatter(xs, ys, zs, c=colors, s=100, alpha=0.8)
            self.node_scatter.set_visible(self.show_nodes)

        # Update node labels.
        for txt in self.node_texts:
            txt.remove()
        self.node_texts = []
        if self.show_node_labels and self.show_nodes:
            for node in visible_nodes:
                if node in draw_pos:
                    x, y, z = draw_pos[node]
                    txt = self.ax.text(x, y, z, node, size=8)
                    self.node_texts.append(txt)

        # Remove previous edge labels.
        for txt in self.edge_texts:
            txt.remove()
        self.edge_texts = []

        # Prepare edge segments.
        all_segments = []
        error_segments = []
        corrections_segment = []
        for u, v, data in self.G.edges(data=True):
            if u not in visible_nodes or v not in visible_nodes:
                continue
            seg = [draw_pos[u], draw_pos[v]]
            all_segments.append(seg)
            if data.get('label', '') in self.error_edges:
                error_segments.append(seg)
            if data.get('label', '') in self.correction_edges:
                corrections_segment.append(seg)
            if not self.show_edge_labels:
                continue
            # Check for parallel edges and space labels if needed
            edges_between = [e for e in self.G.edges(data=True) if (e[0] == u and e[1] == v) or (e[0] == v and e[1] == u)]
            num_parallel = len(edges_between)
            # Find index of current edge among parallel edges
            edge_idx = [i for i, e in enumerate(edges_between) if e[2].get('label', '') == data.get('label', '')]
            idx = edge_idx[0] + 1 if edge_idx else 1
            xm = draw_pos[u][0] + (draw_pos[v][0] - draw_pos[u][0]) / (num_parallel + 1) * idx
            ym = draw_pos[u][1] + (draw_pos[v][1] - draw_pos[u][1]) / (num_parallel + 1) * idx
            zm = draw_pos[u][2] + (draw_pos[v][2] - draw_pos[u][2]) / (num_parallel + 1) * idx
            label = data.get('label', '')
            try:
                etype, eround, eid = self.parse_edge_str(label)
                display_label = f"{eround}-{eid}"
            except Exception:
                display_label = label
            txt = self.ax.text(xm, ym, zm, display_label, size=10, color='black')
            self.edge_texts.append(txt)

        # Update normal edges: show only if "Edges" is enabled.
        if self.show_edges:
            self.normal_edge_collection.set_segments(all_segments)
        else:
            self.normal_edge_collection.set_segments(np.empty((0, 2, 3)))

        # Update error edges independently: drawn if "Errors" is enabled.
        if self.show_errors:
            self.error_edge_collection.set_segments(error_segments)
            self.error_edge_collection.set_color('red')
            self.error_edge_collection.set_linewidth(2.5)
        else:
            self.error_edge_collection.set_segments(np.empty((0, 2, 3)))

        # Update correction edges.
        if self.show_corrections:
            self.correction_edge_collection.set_segments(corrections_segment)
            self.correction_edge_collection.set_color('green')
            self.correction_edge_collection.set_linewidth(1.5)
        else:
            self.correction_edge_collection.set_segments(np.empty((0, 2, 3)))

        # Prepare cluster edges.
        cluster_segments = []
        if self.clusters_dir and self.show_clusters:
            step_clusters = self.clusters.get(self.current_cluster_step, {})
            for u, v, data in self.G.edges(data=True):
                if u not in visible_nodes or v not in visible_nodes:
                    continue
                if data.get('label', '') in step_clusters:
                    cluster_segments.append([draw_pos[u], draw_pos[v]])
        if cluster_segments:
            self.cluster_edge_collection.set_segments(cluster_segments)
            self.cluster_edge_collection.set_color('blue')
            self.cluster_edge_collection.set_linewidth(4)
        else:
            self.cluster_edge_collection.set_segments(np.empty((0, 2, 3)))

        # Manually set x- and y-axis limits based on visible nodes.
        if xs:
            self.ax.set_xlim(min(xs) - 1, max(xs) + 1)
        if ys:
            self.ax.set_ylim(min(ys) - 1, max(ys) + 1)
        # Set z-axis limits.
        if self.single_layer_enabled:
            self.ax.set_zlim(self.selected_layer - 1, self.selected_layer + 1)
        else:
            if zs:
                self.ax.set_zlim(min(zs) - 1, max(zs) + 1)

        # Remove axis ticks.
        self.ax.set_xticks([])
        self.ax.set_yticks([])
        self.ax.set_zticks([])
        self.fig.canvas.draw_idle()

    def toggle_visibility(self, label):
        if label == 'Nodes':
            self.show_nodes = not self.show_nodes
        elif label == 'Node Labels':
            self.show_node_labels = not self.show_node_labels
        elif label == 'Edges':
            self.show_edges = not self.show_edges
        elif label == 'Edge Labels':
            self.show_edge_labels = not self.show_edge_labels
        elif label == 'Errors':
            self.show_errors = not self.show_errors
        elif label == 'Corrections':
            self.show_corrections = not self.show_corrections
        elif label == 'Clusters' and self.clusters_dir:
            self.show_clusters = not self.show_clusters
        elif label == 'Single Layer':
            self.single_layer_enabled = not self.single_layer_enabled
        self.schedule_draw()

    def update_cluster_step(self, val):
        self.current_cluster_step = int(val)
        self.schedule_draw()

    def update_selected_layer(self, label):
        try:
            self.selected_layer = int(label)
        except ValueError:
            self.selected_layer = 0
        self.schedule_draw()

    def top_view(self):
        """Reset the camera view to a top-down perspective."""
        self.ax.view_init(elev=90, azim=-90)
        self.fig.canvas.draw_idle()

    def side_view(self):
        """Reset the camera view to a top-down perspective."""
        self.ax.view_init(elev=0, azim=0)
        self.fig.canvas.draw_idle()


def main():
    parser = argparse.ArgumentParser(
        description="3D Visualize a graph with error overlay, cluster growth, and single layer view."
    )
    parser.add_argument('directory', help='Directory with runs.')
    parser.add_argument('run_id', help='Run to visualize.')
    parser.add_argument('--graph_file', help='Path to the graph file')
    parser.add_argument('--errors_file', help='Path to the errors file')
    parser.add_argument('--decoder', help='Decoder for which to show clustering.')
    parser.add_argument('--clusters', help='Directory containing cluster step files', default=None)
    parser.add_argument("--corrections_file", help="File containing corrections by decoder")
    args = parser.parse_args()

    if not args.graph_file:
        args.graph_file = f"{args.directory}/{args.run_id}/graph.txt"
    if not args.errors_file:
        args.errors_file = f"{args.directory}/{args.run_id}/errors.txt"
    if args.decoder:
        if not args.clusters:
            args.clusters = f"{args.directory}/{args.run_id}/{args.decoder}"
        if not args.corrections_file:
            args.corrections_file = f"{args.directory}/{args.run_id}/{args.decoder}/corrections.txt"

    visualizer = GraphVisualizer3D(args.graph_file, args.errors_file, clusters_dir=args.clusters,
                                   corrections_file=args.corrections_file)

    # Create check buttons.
    rax = plt.axes([0.01, 0.4, 0.2, 0.35])
    check_labels = ['Nodes', 'Node Labels', 'Edges', 'Edge Labels', 'Errors']
    if args.corrections_file:
        check_labels.append('Corrections')
    if args.clusters:
        check_labels.append('Clusters')
    check_labels.append('Single Layer')
    initial = [visualizer.show_nodes, visualizer.show_node_labels,
               visualizer.show_edges, visualizer.show_edge_labels,
               visualizer.show_errors, visualizer.show_corrections]
    if args.clusters:
        initial.append(visualizer.show_clusters)
    initial.append(visualizer.single_layer_enabled)
    check = CheckButtons(rax, check_labels, initial)
    check.on_clicked(visualizer.toggle_visibility)

    # If clusters are provided, add a slider.
    if args.clusters and visualizer.cluster_steps:
        axstep = plt.axes([0.3, 0.1, 0.5, 0.03])
        slider = Slider(axstep, 'Cluster Step',
                        min(visualizer.cluster_steps),
                        max(visualizer.cluster_steps),
                        valinit=min(visualizer.cluster_steps),
                        valstep=1)
        slider.on_changed(visualizer.update_cluster_step)

    # Create concise layer control UI (always visible).
    ax_layer_label = plt.axes([0.01, 0.30, 0.2, 0.04])
    layer_text = ax_layer_label.text(0.5, 0.5, f"Layer: {visualizer.selected_layer}",
                                     transform=ax_layer_label.transAxes, ha="center", va="center")
    ax_layer_label.set_xticks([])
    ax_layer_label.set_yticks([])
    ax_minus = plt.axes([0.01, 0.25, 0.09, 0.04])
    btn_minus = Button(ax_minus, "-")
    ax_plus = plt.axes([0.12, 0.25, 0.09, 0.04])
    btn_plus = Button(ax_plus, "+")
    # Create Reset View button (always visible)
    ax_top = plt.axes([0.01, 0.20, 0.2, 0.04])
    btn_top = Button(ax_top, "Top View")
    # Create Reset View button (always visible)
    ax_side = plt.axes([0.01, 0.15, 0.2, 0.04])
    btn_side = Button(ax_side, "Side View")
    ax_layer_label.set_visible(True)
    ax_minus.set_visible(True)
    ax_plus.set_visible(True)
    ax_top.set_visible(True)
    ax_side.set_visible(True)

    def on_minus(event):
        if visualizer.selected_layer > 0:
            visualizer.selected_layer -= 1
            layer_text.set_text(f"Layer: {visualizer.selected_layer}")
            visualizer.schedule_draw()
            plt.draw()

    def on_plus(event):
        if visualizer.selected_layer < visualizer.total_layers - 1:
            visualizer.selected_layer += 1
            layer_text.set_text(f"Layer: {visualizer.selected_layer}")
            visualizer.schedule_draw()
            plt.draw()

    def on_top(event):
        visualizer.top_view()

    def on_side(event):
        visualizer.side_view()

    btn_minus.on_clicked(on_minus)
    btn_plus.on_clicked(on_plus)
    btn_top.on_clicked(on_top)
    btn_side.on_clicked(on_side)

    plt.show()


if __name__ == '__main__':
    main()
