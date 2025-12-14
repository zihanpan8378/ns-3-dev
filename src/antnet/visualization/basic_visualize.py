
# edges = [
#     (0, 1), (1, 0),
#     (0, 2), (2, 0),
#     (0, 7), (7, 0),
#     (1, 3), (3, 1),
#     (2, 4), (4, 2),
#     (3, 4), (4, 3),
#     (4, 5), (5, 4),
#     (6, 7), (7, 6)
# ]

# visualize_pheromone.py
import argparse
import os
import math
import pandas as pd
import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import glob
import json

def get_topology(ex_dir):
    
    path_to_ip_to_node = ex_dir + "/ip_to_node.json"
    path_to_node_to_ip = ex_dir + "/node_to_ip.json"
    path_to_edges = ex_dir + "/edges.txt"

    with open(path_to_ip_to_node, "r") as f:
        ip_to_node = json.load(f)
    
    with open(path_to_node_to_ip, "r") as f:
        node_to_ip = json.load(f)
    
    namespace = {}
    with open(path_to_edges, "r") as f:
        exec(f.read(), namespace)
    edges = namespace["edges"]

    
    # ip_to_node = {
    #     "10.1.1.1": 1,
    #     "10.1.1.2": 0,
    #     "10.1.2.1": 2,
    #     "10.1.2.2": 0,
    #     "10.1.3.1": 7,
    #     "10.1.3.2": 0,
    #     "10.1.4.1": 3,
    #     "10.1.4.2": 1,
    #     "10.1.5.1": 4,
    #     "10.1.5.2": 2,
    #     "10.1.6.1": 4,
    #     "10.1.6.2": 3,
    #     "10.1.7.1": 5,  
    #     "10.1.7.2": 4,
    #     "10.1.9.1": 7,
    #     "10.1.9.2": 6,
    # }

    # # トポロジ（双方向）
    # edges = [
    #     (0,1),(1,0),
    #     (0,2),(2,0),
    #     (0,7),(7,0),
    #     (1,3),(3,1),
    #     (2,4),(4,2),
    #     (3,4),(4,3),
    #     (4,5),(5,4),
    #     # (5,6),(6,5),
    #     (6,7),(7,6),
    # ]
        
    return ip_to_node, node_to_ip, edges

def get_offset(node, linked_node, pos):
    pos_node = pos[node]
    pos_linked_node = pos[linked_node]
    vector = []
    vector.append(pos_linked_node[0] - pos_node[0])
    vector.append(pos_linked_node[1] - pos_node[1])
    
    offset_x = 0
    offset_y = 0
    
    if vector[0] < 0:
        offset_x -= 0.8
    elif vector[0] == 0:
        offset_x -= 0.3
    else:
        offset_x += 0.2
    
    if vector[1] < 0:
        offset_y -= 0.3
    elif vector[1] == 0:
        offset_y -= 0.05
    else:
        offset_y += 0.2
    
    return offset_x, offset_y

def update(frame_idx, ip_to_node, node_to_ip, pos, G, df, ax, cols, times, destIP):
    ax.clear()
    t = times[frame_idx]
    df_t = df[df["time"] == t]

    pheromone_map = {}

    for _, row in df_t.iterrows():
        src = int(row["node"])
        for col in cols:
            nextHop_ip = col.split("-", 1)[1]
            # print(f"Next hop IP for column {col}: {nextHop_ip}")
            if nextHop_ip not in ip_to_node:
                continue

            dst = ip_to_node[nextHop_ip]
            ph = float(row[col])
            # print(type(ph), ph)
                        
            if math.isnan(ph):
                # print(f"NaN pheromone value for src {src} to dst {dst} at time {t}")
                continue
            
            pheromone_map[(src, dst)] = ph
            print(src, "->", dst, ":", ph)
    widths = []
    for u, v in G.edges():
        w = pheromone_map.get((u, v), 0.0)
        # print(w)
        widths.append(max(0.1, w * 6.0))
    # print(widths)

    nx.draw_networkx_nodes(G, pos=pos, ax=ax, node_size=900, node_color="lightblue")
    nx.draw_networkx_labels(G, pos=pos, ax=ax)
    nx.draw_networkx_edges(G, 
                           pos=pos, 
                           ax=ax, 
                           width=widths, 
                           edge_color="gray", 
                           arrows=True, 
                           arrowsize=20,
                           connectionstyle='arc3,rad=0.1')

    for node in range(len(pos)):
        x, y = pos[node]
        ip_list = node_to_ip[str(node)]
        for ip in ip_list:
            linked_node = ip_to_node[ip]
            offset_x, offset_y = get_offset(node, linked_node, pos)
            ax.text(x + offset_x, y + offset_y, ip, fontsize=12)
            
    # x, y = pos[0]
    # ax.text(x + 0.2, y + 0.2, "10.1.1.1", fontsize=12)
    # ax.text(x + 0.2, y - 0.3, "10.1.2.1", fontsize=12)
    # ax.text(x - 0.8, y, "10.1.3.1", fontsize=12)
    
    # x, y = pos[1]
    # ax.text(x - 0.8, y + 0.2, "10.1.1.2", fontsize=12)
    # ax.text(x + 0.2, y - 0.4, "10.1.4.1", fontsize=12)
    
    # x, y = pos[2]
    # ax.text(x + 0.2, y + 0.2, "10.1.2.2", fontsize=12)
    # ax.text(x + 0.2, y - 0.2, "10.1.5.1", fontsize=12)
    
    # x, y = pos[3]
    # ax.text(x + 0.2, y + 0.2, "10.1.4.2", fontsize=12)
    # ax.text(x - 1.0, y - 0.1, "10.1.6.1", fontsize=12)
    
    # x, y = pos[4]
    # ax.text(x - 0.8, y + 0.2, "10.1.5.2", fontsize=12)
    # ax.text(x + 0.2, y + 0.2, "10.1.6.2", fontsize=12)
    # ax.text(x + 0.2, y - 0.3, "10.1.7.1", fontsize=12)
    
    # x, y = pos[5]
    # ax.text(x - 0.8, y + 0.2, "10.1.7.2", fontsize=12)
    
    # x, y = pos[6]
    # ax.text(x + 0.2, y + 0.2, "10.1.9.1", fontsize=12)
    
    # x, y = pos[7]
    # ax.text(x - 0.4, y + 0.3, "10.1.3.2", fontsize=12)
    # ax.text(x + 0.2, y - 0.2, "10.1.9.2", fontsize=12)

    ax.set_title(f"Pheromone toward {destIP} — time={t}", fontsize=20)
    ax.axis("off")

def visualize(ex_dir, destIP, outputDir, pos):
    
    
    ip_to_node, node_to_ip, edges = get_topology(ex_dir)
    
    # =======================
    #  CSV の統合
    # =======================
    csv_dir = ex_dir + "/pheromones/"
    print(csv_dir)
    files = sorted(glob.glob(os.path.join(csv_dir, "_node_*.csv")))
    if len(files) == 0:
        raise RuntimeError("No node*.csv found in directory.")

    df_list = []

    for path in files:
        # node番号を抽出
        base = os.path.basename(path)
        node_id = int(base.replace("_node_", "").replace(".csv", ""))

        df_node = pd.read_csv(path)
        df_node["node"] = node_id   # node列を追加

        df_list.append(df_node)

    # 全ノードの CSV を1つに統合
    df = pd.concat(df_list, ignore_index=True)
    
    # =======================
    # 可視化準備
    # =======================

    cols = [c for c in df.columns if c.startswith(destIP)]
    times = sorted(df["time"].unique())
    skip = 5
    times = times[::skip]
    all_nodes = sorted(df["node"].unique())

    # colsの中身をfor文で確認する
    for col in cols:
        print(f"Column for dest {destIP}: {col}")

    G = nx.DiGraph()
    G.add_nodes_from(all_nodes)
    for u, v in edges:
        G.add_edge(u, v)

    fig, ax = plt.subplots(figsize=(10, 7))
    
    
    
    fargs = (ip_to_node, node_to_ip, pos, G, df, ax, cols, times, destIP)
    ani = animation.FuncAnimation(fig, update, frames=len(times), fargs=fargs, interval=100, repeat=True)

    # plt.show()
    # ani.save(os.path.join(output_dir, "pheromone_animation.gif"), dpi=150)
    ani.save(os.path.join(outputDir, "pheromone_animation.mp4"), dpi=150)
    

def main(pos):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--exName",
        type=str,
        default="ex2",
    )
    parser.add_argument(
        "--destIP",
        type=str,
        default="10.1.7.2",
    )
    parser.add_argument(
        "--outputDir",
        type=str,
        default="src/antnet/visualization"
    )
    
    args = parser.parse_args()
    
    ex_dir = "src/antnet/log/" + args.exName
    
    if args.destIP == "":
        print("specify dest IP address to visualize pheromones")
        
    visualize(ex_dir=ex_dir, destIP=args.destIP, outputDir=args.outputDir, pos=pos)

if __name__ == "__main__":
    # ノードの座標
    pos = {
        0: (0,  3),
        1: (3,  3),
        7: (-3, 1),
        2: (0,  1),
        3: (3,  1),
        6: (-3,-1),
        4: (0, -1),
        5: (3, -1),
    }
    
    # output_dir = "src/antnet/visualization"  # 可視化結果の保存先ディレクトリ
    # csv_dir = "src/antnet/log/pheromones/"       # node0.csv などが入っているディレクトリ
    # dest_target = "10.1.7.2"        # 可視化したい destination
    
    
    ## things to implement
    # update → from pos, ip_to_node, edges → firgure out the where to put the ip address
    # get_topology → from csv get ip_to_node and edges
    # for build_ant_topology → export ip_to_node and edges as csv
    
    main(pos)