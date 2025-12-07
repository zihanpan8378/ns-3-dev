
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
import math
import pandas as pd
import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import glob
import os

# =======================
# ユーザ設定
# =======================
output_dir = "src/antnet/visualization"  # 可視化結果の保存先ディレクトリ
csv_dir = "src/antnet/log/pheromones/"       # node0.csv などが入っているディレクトリ
dest_target = "10.1.7.2"        # 可視化したい destination

# IP → node mapping（あなたの環境に合わせて修正）
# ip_to_node = {
#     "10.1.1.1": 0,
#     "10.1.1.2": 1,
#     "10.1.2.1": 0,
#     "10.1.2.2": 2,
#     "10.1.3.1": 0,
#     "10.1.3.2": 7,
#     "10.1.4.1": 1,
#     "10.1.4.2": 3,
#     "10.1.5.1": 2,
#     "10.1.5.2": 4,
#     "10.1.6.1": 3,
#     "10.1.6.2": 4,
#     "10.1.7.1": 4,  
#     "10.1.7.2": 5,
#     "10.1.9.1": 6,
#     "10.1.9.2": 7,
# }
ip_to_node = {
    "10.1.1.1": 1,
    "10.1.1.2": 0,
    "10.1.2.1": 2,
    "10.1.2.2": 0,
    "10.1.3.1": 7,
    "10.1.3.2": 0,
    "10.1.4.1": 3,
    "10.1.4.2": 1,
    "10.1.5.1": 4,
    "10.1.5.2": 2,
    "10.1.6.1": 4,
    "10.1.6.2": 3,
    "10.1.7.1": 5,  
    "10.1.7.2": 4,
    "10.1.9.1": 7,
    "10.1.9.2": 6,
}

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

# トポロジ（双方向）
edges = [
    (0,1),(1,0),
    (0,2),(2,0),
    (0,7),(7,0),
    (1,3),(3,1),
    (2,4),(4,2),
    (3,4),(4,3),
    (4,5),(5,4),
    # (5,6),(6,5),
    (6,7),(7,6),
]

# =======================
#  CSV の統合
# =======================

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

# dfの全ての列の中身をfor文で確認する
# for col in df.columns:
#     print(f"Column: {col}")

# =======================
# 可視化準備
# =======================

cols = [c for c in df.columns if c.startswith(dest_target)]
times = sorted(df["time"].unique())
all_nodes = sorted(df["node"].unique())

# colsの中身をfor文で確認する
for col in cols:
    print(f"Column for dest {dest_target}: {col}")

G = nx.DiGraph()
G.add_nodes_from(all_nodes)
for u, v in edges:
    G.add_edge(u, v)

fig, ax = plt.subplots(figsize=(10, 7))

# =======================
# アニメーション更新
# =======================
def update(frame_idx):
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
        widths.append(max(0.2, w * 6.0))
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

    ax.set_title(f"Pheromone toward {dest_target} — time={t}")
    ax.axis("off")

ani = animation.FuncAnimation(fig, update, frames=len(times), interval=100, repeat=True)

plt.show()
# ani.save(os.path.join(output_dir, "pheromone_animation.gif"), dpi=150)
