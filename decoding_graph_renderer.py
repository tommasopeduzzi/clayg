import argparse
from math import ceil
import matplotlib
from matplotlib.widgets import CheckButtons, Button
from networkx import node_expansion

matplotlib.use("Qt5Agg")  # or "Qt5Agg", "GTK3Agg", "WebAgg" depending on your system
import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
from mpl_toolkits.mplot3d import Axes3D
from itertools import cycle

# Parse arguments
parser = argparse.ArgumentParser()
parser.add_argument("run_id", help="run id to visualize")
parser.add_argument("output", help="output file")
args = parser.parse_args()

graph_file = f"runs/{args.run_id}/graph.txt"
def clusters_file(step):
    return  f"runs/{args.run_id}/clusters_{step}.txt"

def parse_node(node):
    type, round, id = node.split('-')
    match int(type):
        case 0:
            type = "Ancilla"
        case 1:
            type = "Virtual"
    return type, int(round), int(id)

def parse_edge(edge):
    type, round, id = edge.split('-')
    match int(type):
        case 0:
            type = "Measurement"
        case 1:
            type = "Normal"
    return type, int(round), int(id)

# Read graph
graph = []
with open(graph_file, "r") as f:
    for line in f:
        constituents = line.strip().split(',')
        graph.append((parse_node(constituents[0]), parse_node(constituents[1]), parse_edge(constituents[2])))

# Construct graph
G = nx.Graph()
edges = {}
edge_id_to_tuple = {}
for node1, node2, edge in graph:
    G.add_edge(node1, node2)
    edge_tuple = frozenset({node1, node2})
    edges[edge_tuple] = edge
    edge_id_to_tuple[edge] = edge_tuple

# Read cluster file
def parse_clusters(clusters_file):
    new_edge_clusters = {}
    with open(clusters_file, "r") as f:
        for line in f:
            edge_id, cluster_id = line.strip().split(",")
            new_edge_clusters[parse_edge(edge_id)] = int(cluster_id)
    return new_edge_clusters

clusters_step = 0
edge_clusters = parse_clusters(clusters_file(clusters_step))

# Generate colors dynamically
unique_clusters = sorted(set(edge_clusters.values()))
cmap = plt.get_cmap("hsv")
colors_map = {cluster: cmap(i / max(1, len(unique_clusters) - 1)) for i, cluster in enumerate(unique_clusters)}

# Compute layout
rounds = max([node[1] for node in G.nodes()]) + 1
distance = len([1 for node1, node2 in G.edges() if node1[1] == 0 and node2[1] == 0])**0.5
nodes_per_layer = ceil(distance / 2)

node_colors = []
pos = {}
for type, round, id in G.nodes():
    node_colors.append("red" if type == "Ancilla" else "blue")
    if type == "Virtual":
        if id < nodes_per_layer:
            x = id - 0.5
            y = 0
        else:
            x = id % nodes_per_layer
            y = distance
        z = (rounds - 1) / 2
    else:
        x = id % nodes_per_layer - (id // nodes_per_layer) % 2 * 0.5
        y = id // nodes_per_layer + 1
        z = round
    pos[(type, round, id)] = (x * 3, y * 3, z)

# Create 3D plot
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

# Draw nodes
for i, node in enumerate(G.nodes()):
    x, y, z = pos[node]
    ax.scatter(x, y, z, color=node_colors[i], s=50)

all_edges = []
normal_edges = []
node_labels = []
edge_labels = []
# Draw edges with cluster colors
for node1, node2 in G.edges():
    edge_tuple = frozenset({node1, node2})
    edge = edges[edge_tuple]
    x1, y1, z1 = pos[node1]
    x2, y2, z2 = pos[node2]
    cluster = edge_clusters.get(edge, -1)
    color = colors_map.get(cluster, "black")
    alpha = 0.5 if cluster == -1 else 1
    line_list = ax.plot([x1, x2], [y1, y2], [z1, z2], color=color, alpha=alpha, linewidth=1, linestyle='dotted' if edge[0] == "Measurement" else 'solid')
    all_edges.append(line_list)
    if cluster == -1:
        normal_edges.append(line_list)
    if edge[0] == "Normal":
        edge_labels.append(ax.text((x1+x2)/2, (y1+y2)/2, (z1+z2)/2, f"{edge[1]}-{edge[2]}", size=10, zorder=1))

# Labels
for node in G.nodes():
    x, y, z = pos[node]
    node_labels.append(ax.text(x, y, z, f"{node[2]}", size=10, zorder=1))

def toggle_visibility(label):
    if label == "Edges":
        for line_list in normal_edges:
            for line in line_list:
                line.set_visible(not line.get_visible())
    elif label == "Edge Labels":
        for text in edge_labels:
            text.set_visible(not text.get_visible())
    elif label == "Node Labels":
        for text in node_labels:
            text.set_visible(not text.get_visible())
    plt.draw()

# Create checkboxes
plt.subplots_adjust(left=0.25)  # Adjust layout for checkbox placement
ax_checkbox = plt.axes((0.02, 0.5, 0.15, 0.15))  # Position checkbox panel
check = CheckButtons(ax_checkbox, ["Edges", "Edge Labels", "Node Labels"], [True, True, True])
ax_checkbox.set_title("Visibility")
check.on_clicked(toggle_visibility)

# Add arrow navigation button
def move_left(event):
    ax.view_init(elev=ax.elev, azim=ax.azim - 10)
    plt.draw()

def move_right(event):
    ax.view_init(elev=ax.elev, azim=ax.azim + 10)
    plt.draw()

def move_up(event):
    ax.view_init(elev=ax.elev + 10, azim=ax.azim)
    plt.draw()

def move_down(event):
    ax.view_init(elev=ax.elev - 10, azim=ax.azim)
    plt.draw()

ax_left = plt.axes([0.02, 0.4, 0.1, 0.04])
btn_left = Button(ax_left, 'Left')
btn_left.on_clicked(move_left)

ax_right = plt.axes([0.14, 0.4, 0.1, 0.04])
btn_right = Button(ax_right, 'Right')
btn_right.on_clicked(move_right)

ax_up = plt.axes([0.08, 0.45, 0.1, 0.04])
btn_up = Button(ax_up, 'Up')
btn_up.on_clicked(move_up)

ax_down = plt.axes([0.08, 0.35, 0.1, 0.04])
btn_down = Button(ax_down, 'Down')
btn_down.on_clicked(move_down)


# Add cluster navigation button
def update_cluster():
    """ Updates the cluster visualization based on the current clusters_step """
    global normal_edges, all_edges, edge_labels
    # Read the new cluster file based on clusters_step
    edge_clusters = parse_clusters(clusters_file(clusters_step))

    # Update edge colors dynamically
    unique_clusters = sorted(set(edge_clusters.values()))
    cmap = plt.get_cmap("hsv")
    colors_map = {cluster: cmap(i / max(1, len(unique_clusters) - 1)) for i, cluster in enumerate(unique_clusters)}

    # Redraw edges with new cluster colors
    for line_list in all_edges:
        for line in line_list:
            line.set_visible(False)  # Hide the previous edges before redrawing
    for text in edge_labels:
        text.set_visible(False)

    all_edges = []
    normal_edges = []
    edge_labels = []
    # Draw edges with updated colors
    for node1, node2 in G.edges():
        edge_tuple = frozenset({node1, node2})
        edge = edges[edge_tuple]
        x1, y1, z1 = pos[node1]
        x2, y2, z2 = pos[node2]
        cluster = edge_clusters.get(edge, -1)
        color = colors_map.get(cluster, "black")
        alpha = 0.5 if cluster == -1 else 1
        line_list = ax.plot([x1, x2], [y1, y2], [z1, z2], color=color, alpha=alpha, linewidth=1, linestyle='dotted' if edge[0] == "Measurement" else 'solid')
        all_edges.append(line_list)
        if cluster == -1:
            normal_edges.append(line_list)
        if edge[0] == "Normal":
            edge_labels.append(ax.text((x1+x2)/2, (y1+y2)/2, (z1+z2)/2, f"{edge[1]}-{edge[2]}", size=10, zorder=1))
    plt.draw()

    if not check.get_status()[0]:
        toggle_visibility("Edges")
    if not check.get_status()[1]:
        toggle_visibility("Edge Labels")

def next_cluster(event):
    """ Move to the next cluster and update the graph visualization """
    global clusters_step
    clusters_step += 1
    print(f"Switching to cluster {clusters_step}")
    update_cluster()  # Update the graph visualization with the new cluster


def prev_cluster(event):
    """ Move to the previous cluster and update the graph visualization """
    global clusters_step
    clusters_step = max(0, clusters_step - 1)
    print(f"Switching to cluster {clusters_step}")
    update_cluster()  # Update the graph visualization with the new cluster

ax_next_cluster = plt.axes([0.02, 0.3, 0.2, 0.04])
btn_next_cluster = Button(ax_next_cluster, 'Next Cluster')
btn_next_cluster.on_clicked(next_cluster)

ax_prev_cluster = plt.axes([0.02, 0.25, 0.2, 0.04])
btn_prev_cluster = Button(ax_prev_cluster, 'Previous Cluster')
btn_prev_cluster.on_clicked(prev_cluster)

# Show plot
plt.show()
plt.savefig(args.output)