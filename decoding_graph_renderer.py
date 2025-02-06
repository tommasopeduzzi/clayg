import argparse
import os
from math import ceil

import matplotlib
from matplotlib.widgets import CheckButtons, Button

matplotlib.use("Qt5Agg")  # or "Qt5Agg", "GTK3Agg", "WebAgg" depending on your system
import matplotlib.pyplot as plt
import networkx as nx

# Parse arguments
parser = argparse.ArgumentParser()
parser.add_argument("run_id", help="run id to visualize")
parser.add_argument("--clusters", help="cluster file to visualize", default="")
parser.add_argument("--parent_folder", help="parent folder of the runs", default="/runs/")
parser.add_argument("output", help="output file")
args = parser.parse_args()

edge_visibility = True
edge_labels_visibility = False
node_labels_visibility = False

graph_file = f"{args.parent_folder}/{args.run_id}/graph.txt"
marked_nodes_file = f"{args.parent_folder}/{args.run_id}/marked_nodes.txt"
error_edges_file = f"{args.parent_folder}/{args.run_id}/errors.txt"
def clusters_file(step):
    assert not args.clusters == ""
    return f"{args.parent_folder}/{args.run_id}/{args.clusters}/clusters_{step}.txt"

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
edge_id_to_tuple = {}
for node1, node2, edge in graph:
    G.add_edge(node1, node2, edge=edge)

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

error_edges = []
with open(error_edges_file, "r") as f:
    for line in f:
        error_edges.append(parse_edge(line.strip()))

all_edges = {}
edge_labels = []
# Draw edges with cluster colors
clusters_step = 0
# Read cluster file
def parse_clusters(clusters_file):
    new_edge_clusters = {}
    with open(clusters_file, "r") as f:
        for line in f:
            edge_id, cluster_id = line.strip().split(",")
            new_edge_clusters[parse_edge(edge_id)] = int(cluster_id)
    return new_edge_clusters

def update_cluster():
    """ Updates the cluster visualization based on the current clusters_step """
    # Read the new cluster file based on clusters_step
    edge_clusters = parse_clusters(clusters_file(clusters_step))

    # Update edge colors dynamically
    unique_clusters = sorted(set(edge_clusters.values()))
    cmap = plt.get_cmap("hsv")
    colors_map = {cluster: cmap(i / max(1, len(unique_clusters) - 1)) for i, cluster in enumerate(unique_clusters)}

    # Draw edges with updated colors
    for edge, line in all_edges.items():
        cluster = edge_clusters.get(edge, -1)
        color = colors_map.get(cluster, "black")
        alpha = 0.5 if cluster == -1 else 1
        line.set_color(color)
        line.set_alpha(alpha)
        if cluster == -1:
            line.set_visible(edge_visibility)
        else:
            line.set_visible(True)

for node1, node2, edge in G.edges().data("edge"):
    x1, y1, z1 = pos[node1]
    x2, y2, z2 = pos[node2]
    line = ax.plot([x1, x2], [y1, y2], [z1, z2], linewidth=1, alpha=0.5, color="black",
                   linestyle='dotted' if edge[0] == "Measurement" else 'solid')[0]
    all_edges[edge] = line
    if edge in error_edges:
        # second thick red line
        ax.plot([x1, x2], [y1, y2], [z1, z2], linewidth=2, color="red", zorder=-1)
    if edge[0] == "Normal":
        edge_labels.append(ax.text((x1+x2)/2, (y1+y2)/2, (z1+z2)/2, f"{edge[1]}-{edge[2]}", size=10, zorder=1, visible=edge_labels_visibility))

if not args.clusters == "":
    update_cluster()

# Node Labels
node_labels = []
for node in G.nodes():
    x, y, z = pos[node]
    node_labels.append(ax.text(x, y, z, f"{node[2]}", size=10, zorder=1, visible=node_labels_visibility))

def toggle_visibility(label):
    global edge_visibility, edge_labels_visibility, node_labels_visibility
    if label == "Edges":
        edge_visibility = not edge_visibility
        if args.clusters == "":
            edge_clusters = {}
        else:
            edge_clusters = parse_clusters(clusters_file(clusters_step))
        for edge, line in all_edges.items():
            cluster = edge_clusters.get(edge, -1)
            if cluster == -1 and not edge in error_edges:
                line.set_visible(not line.get_visible())
    elif label == "Edge Labels":
        edge_labels_visibility = not edge_labels_visibility
        for text in edge_labels:
            text.set_visible(not text.get_visible())
    elif label == "Node Labels":
        node_labels_visibility = not node_labels_visibility
        for text in node_labels:
            text.set_visible(not text.get_visible())
    plt.draw()

# Create checkboxes
plt.subplots_adjust(left=0.25)  # Adjust layout for checkbox placement
ax_checkbox = plt.axes((0.02, 0.5, 0.2, 0.15))  # Position checkbox panel
check = CheckButtons(ax_checkbox, ["Edges", "Edge Labels", "Node Labels"], [edge_visibility, edge_labels_visibility, node_labels_visibility])
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


# navigation panel
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
def next_cluster(event):
    """ Move to the next cluster and update the graph visualization """
    global clusters_step
    # get max cluster from files in directory
    clusters_step = min(clusters_step + 1,
                        len([cluster_file for cluster_file in os.listdir("runs/" + args.run_id)
                             if cluster_file.startswith("clusters_")]) - 1)
    print(f"Switching to cluster {clusters_step}")
    update_cluster()  # Update the graph visualization with the new cluster
    plt.draw()

def prev_cluster(event):
    """ Move to the previous cluster and update the graph visualization """
    global clusters_step
    clusters_step = max(0, clusters_step - 1)
    print(f"Switching to cluster {clusters_step}")
    update_cluster()  # Update the graph visualization with the new cluster
    plt.draw()


if not args.clusters == "":
    ax_next_cluster = plt.axes([0.02, 0.3, 0.2, 0.04])
    btn_next_cluster = Button(ax_next_cluster, 'Next Cluster')
    btn_next_cluster.on_clicked(next_cluster)

    ax_prev_cluster = plt.axes([0.02, 0.25, 0.2, 0.04])
    btn_prev_cluster = Button(ax_prev_cluster, 'Previous Cluster')
    btn_prev_cluster.on_clicked(prev_cluster)

# Show plot
plt.show()