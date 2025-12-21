import json

import matplotlib.pyplot as plt


def plot_at_most_one_node_fail_latency(file_path, label_list, save_path):
    fd = open(file_path, 'r')
    data_list = json.loads(fd.readline())

    color = ["red", "blue", "orange", "green"]
    x = []
    label_list[0] = "Baseline without link failure"
    for i in range(len(data_list[0])):
        x.append(i * 10)

    plt.axvline(x=60, linestyle='--', linewidth=2, label='_nolegend_')
    plt.axvline(x=180, color='red', linestyle='--', linewidth=2, label='_nolegend_')
    plt.xlabel("Time (s)")

    for i in range(len(data_list)):
        plt.plot(x, data_list[i], label=label_list[i], color=color[i])

    plt.ylabel("Avg Latency (ms)")

    plt.title("Average Latency Every Min With Node Failure")
    plt.ylim(48, 73)

    plt.grid(True, which="both", axis="both")
    plt.minorticks_on()
    plt.grid(True, which="major", axis="both")
    plt.grid(True, which="minor", axis="both", alpha=0.25)

    plt.legend(loc='upper right')
    plt.savefig(save_path)
    plt.close()
    return data_list


def plot_mobile_latency(file_path, label_list, save_path):
    color = ["red", "blue", "orange", "green"]
    fd = open(file_path, 'r')
    data_list = json.loads(fd.readline())

    x = []
    for i in range(len(data_list[0])):
        x.append(i * 10)

    plt.xlabel("Time (s)")
    plt.ylim(48, 68)

    for i in range(len(data_list)):
        plt.plot(x, data_list[i], label=label_list[i], color=color[i])


    plt.ylabel("Avg Latency (ms)")

    plt.title("Average Latency Every Min With Node Mobility")
    plt.grid(True, which="both", axis="both")
    plt.minorticks_on()
    plt.grid(True, which="major", axis="both")
    plt.grid(True, which="minor", axis="both", alpha=0.25)

    plt.legend(loc='upper right')
    plt.savefig(save_path)
    plt.close()
    return data_list

def plot_stable_lost_rate(file_path, label_list, save_path):
    color = ["red", "blue", "orange", "green"]
    fd = open(file_path, 'r')
    data_list = json.loads(fd.readline())


    x = []
    for i in range(len(data_list[0])):
        x.append(i * 10)

    label_list[0] = "Baseline without interference"
    plt.xlabel("Time (s)")
    plt.ylim(48, 88)

    for i in range(len(data_list)):
        plt.plot(x, data_list[i], label=label_list[i], color=color[i])


    plt.ylabel("Avg Latency (ms)")

    plt.title("Average Latency Every Min With Stable Loss Rate")

    plt.grid(True, which="both", axis="both")
    plt.minorticks_on()
    plt.grid(True, which="major", axis="both")
    plt.grid(True, which="minor", axis="both", alpha=0.25)

    plt.legend(loc='upper right')
    plt.savefig(save_path)
    plt.close()
    return data_list

# -----------------------------
# 强制 x 轴贴边：无左右空隙（关键）
# -----------------------------
def _tight_xlim(ax, x):
    xmin, xmax = min(x), max(x)

    # 1) 关掉 sticky edges（不然会自动留白）
    ax.use_sticky_edges = False

    # 2) 关掉 x margin
    ax.margins(x=0)
    ax.set_xmargin(0)

    # 3) 强制 xlim = 数据首尾
    ax.set_xlim(xmin, xmax)

    # 4) 锁住 x 轴，后续 plot 不再把 xlim 改回去
    ax.autoscale(enable=False, axis="x")


def loss_and_latency_vs_threshold_one_fig(loss_rate_path, avg_path, save_path):
    x = [i for i in range(10, 110, 10)]  # 10..100
    fd = open(loss_rate_path, 'r')
    y_loss = json.loads(fd.readline())
    fd.close()

    fd = open(avg_path, 'r')
    y_lat = json.loads(fd.readline())
    fd.close()

    fig, ax1 = plt.subplots()
    c1, c2 = "tab:blue", "tab:orange"

    line1, = ax1.plot(x, y_loss, color=c1, label="Packet Loss Rate (%)")
    ax1.set_xlabel("Notification Threshold (%)")
    ax1.set_ylabel("Packet Loss Rate (%)")

    ax2 = ax1.twinx()
    line2, = ax2.plot(x, y_lat, color=c2, label="Avg Latency (ms)")
    ax2.set_ylabel("Avg Latency (ms)")
    ax2.set_ylim(56, 65)

    _tight_xlim(ax1, x)

    ax1.minorticks_on()
    ax1.grid(True, which="major", axis="both")
    ax1.grid(True, which="minor", axis="both", alpha=0.25)

    ax1.set_title("Packet Loss Rate & Avg Latency v.s. Notification Threshold")

    lines = [line1, line2]
    ax1.legend(lines, [l.get_label() for l in lines], loc="best")

    plt.tight_layout()
    plt.savefig(save_path, dpi=300)
    plt.close()


def loss_and_latency_vs_size_one_fig(loss_rate_path, avg_path, save_path):
    x = [i for i in range(1, 14)]
    fd = open(loss_rate_path, 'r')
    y_loss = json.loads(fd.readline())
    fd.close()


    fd = open(avg_path, 'r')
    y_lat = json.loads(fd.readline())
    fd.close()


    fig, ax1 = plt.subplots()
    c1, c2 = "tab:blue", "tab:orange"

    line1, = ax1.plot(x, y_loss, color=c1, label="Packet Loss Rate (%)")
    ax1.set_xlabel("Beacon Window Size")
    ax1.set_ylabel("Packet Loss Rate (%)")

    ax2 = ax1.twinx()
    line2, = ax2.plot(x, y_lat, color=c2, label="Avg Latency (ms)")
    ax2.set_ylabel("Avg Latency (ms)")

    x_all = x
    _tight_xlim(ax1, x_all)

    ax1.minorticks_on()
    ax1.grid(True, which="major", axis="both")
    ax1.grid(True, which="minor", axis="both", alpha=0.25)

    ax1.set_title("Packet Loss Rate & Avg Latency v.s. Beacon Window Size")

    lines = [line1, line2]
    ax1.legend(lines, [l.get_label() for l in lines], loc="best")

    plt.tight_layout()
    plt.savefig(save_path, dpi=300)
    plt.close()

# ============================================================
# 3) Period: Loss rate (left) + Latency (right)
# ============================================================
def loss_and_latency_vs_period_one_fig(loss_rate_path, avg_path, save_path):
    x = [i / 10 for i in range(1, 31, 1)]  # 0.1..3.0
    fd = open(loss_rate_path, 'r')
    y_loss = json.loads(fd.readline())
    fd.close()


    fd = open(avg_path, 'r')
    y_lat = json.loads(fd.readline())
    fd.close()


    fig, ax1 = plt.subplots()
    c1, c2 = "tab:blue", "tab:orange"

    line1, = ax1.plot(x, y_loss, color=c1, label="Packet Loss Rate (%)")
    ax1.set_xlabel("Beacon Sending Interval (s)")
    ax1.set_ylabel("Packet Loss Rate (%)")

    ax2 = ax1.twinx()
    line2, = ax2.plot(x, y_lat, color=c2, label="Avg Latency (ms)")
    ax2.set_ylabel("Avg Latency (ms)")

    _tight_xlim(ax1, x)

    ax1.minorticks_on()
    ax1.grid(True, which="major", axis="both")
    ax1.grid(True, which="minor", axis="both", alpha=0.25)

    ax1.set_title("Packet Loss Rate & Avg Latency v.s. Beacon Sending Interval")

    lines = [line1, line2]
    ax1.legend(lines, [l.get_label() for l in lines], loc="best")

    plt.tight_layout()
    plt.savefig(save_path, dpi=300)
    plt.close()