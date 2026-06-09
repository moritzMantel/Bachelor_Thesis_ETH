import subprocess
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np
import os
import math
import csv
import time

BINARY = "./app"
DATA_DIR = "evaluation/data"
FIG_DIR = "evaluation/figures"

PROC_F_GHZ = 3.0
x = int("A111F275CC2E7588000001D300A31E76336D15B9D314CD1A1D8F3D3556975EED", 16)
N = int("A708C278C8067461B50CD7F104FE4A612A8C53049AE543802914515155A753CFF168C03A64B793FE325C3FC7BEBFB4BFB405AF92EA6993689CA93C71378B9FFD2EFEC803BB61C3B997F7D7D170BEE8D8DF5F943E2D77D936E8946BFC10C59E577F16E90BE67C3185637C9B8AFFD58F7B11F21A6659160C38D6B0E7F7245680860DAF92C12B64E13FFAF10B6A7FC717E905CD19D728FE1F4BA94FFFA6AD9CDDDB58B62631B4FD3E816BF99311132DFB3D0F99596AD1E115EDED8A315E051B12C5BF1A1BA40A2E090CAA4D6B45CF22BD820FF52CA22231A36F63EA5E57623CF8E61A66C611C57BF08902894E75ACEE14C16CC44BBFA86A398708A523DC92F46781", 16)

def measure(T):
    for _ in range(5):
        pow(x, 2**T, N)
    
    start = time.perf_counter_ns()
    for i in range(20):
        pow(x, 2**T, N)
    elapsed_ns = time.perf_counter_ns() - start
    return (int((elapsed_ns / 20) * PROC_F_GHZ), int(elapsed_ns / 20000))

def run_once(
    elements,
    log=0,
    log_file=None,
    num_bits=1024,
    private_set_digits=4,
    private_set_size=100,
    min_expected_cycles=0,
    T_exp=15,
    T_baseline_comp=0,
    T_input_size_comp=0,
    T_private_set_comp=0,
):
    """
    Calls the binary once with the given config and elements.
    Appends one row to log_file if log >= 2.
    """
    args = [
        BINARY,
        "1",                            # do_config = 1 always
        str(num_bits),
        str(private_set_digits),
        str(private_set_size),
        str(min_expected_cycles),
        str(T_exp),
        str(T_baseline_comp),
        str(T_input_size_comp),
        str(T_private_set_comp),
        str(log),
    ]
    if log >= 2:
        args.append(log_file)
    args.append(str(len(elements)))
    args += [str(e) for e in elements]

    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        print("stderr:", result.stderr)
        print("stdout:", result.stdout)
        raise RuntimeError(f"Binary exited with code {result.returncode}")
    if log == 1:
        print(result.stdout)


def gather_data():
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(FIG_DIR, exist_ok=True)

    # -- experiment 1: vary T_exp, single element request --
    # out = f"{DATA_DIR}/vary_T_exp.csv"
    # if os.path.exists(out):
    #     os.remove(out)
    # for T_exp in range(1, 21):
    #     run_once(
    #         elements=[42],
    #         log=2,
    #         log_file="vary_T_exp",
    #         T_exp=T_exp,
    #         T_baseline_comp=0,
    #         T_input_size_comp=0,
    #         T_private_set_comp=0,
    #         private_set_size=100,
    #     )
    #     print(f"vary_T_exp: T_exp={T_exp} done")

    # -- experiment 2: vary input size, constant T vs linear T --
    # out = f"{DATA_DIR}/vary_input_size.csv"
    # if os.path.exists(out):
    #     os.remove(out)
    # for T_isc in [0, 1]:
    #     for n in range(1, 100, 10):
    #         elements = list(range(n))
    #         run_once(
    #             elements=elements,
    #             log=2,
    #             log_file="vary_input_size",
    #             T_exp=10,
    #             T_baseline_comp=0,
    #             T_input_size_comp=T_isc,
    #             T_private_set_comp=0,
    #             private_set_size=100,
    #         )
    #     print(f"vary_input_size: T_isc={T_isc} n={n} done")

    # -- experiment 3: cycle measurements --
    # out = f"{DATA_DIR}/cycles.csv.cycles.csv"
    # if os.path.exists(out):
    #     os.remove(out)
    # for t in range(5, 20):
    #     run_once(
    #         elements=[42],
    #         log=3,
    #         log_file="cycles",
    #         T_exp=t,
    #         T_baseline_comp=0,
    #         T_input_size_comp=0,
    #         T_private_set_comp=0,
    #         private_set_size=100,
    #     )
    # print("cycles done")
    with open(f"{DATA_DIR}/cycles_py_2.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["T","Time","Cycles"])
        for t in range(5,35):
            cycles, ms = measure(t)
            writer.writerow([2**t + 7, ms, cycles])
    



def plot_T(df, y_cols, title, ylabel, xlabel, logy, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    df["T"] = 2 ** df["T_exp"]
    colors = cm.tab10(np.linspace(0, 1, len(y_cols)))
    for y_col, color in zip(y_cols, colors):
        y = df[y_col].clip(lower=1)
        ax.plot(df["T"], y, marker="o", label=y_col, color=color)

    if logy:
        ax.set_yscale("log", base=2)
    ax.set_ylabel(ylabel)


    ax.set_xscale("log", base=2)
    ax.set_xlim(left=1)
    ax.set_xlabel(xlabel)

    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    row = df.iloc[0]
    config = [
        f"Modulus Size:  1024 bits",
        f"Size of Input:  {row["RequestSize"]} elem",
        f"Size of Domain:  {10**row["private_set_digits"]}",
        f"Size of Private Set:  {row["private_set_size"]}",
        f"Dynamic Scaling of T:\n  - No scaling of T"
    ]
    config_text = "\n".join(config)
    ax.text(
        0.02, 0.98, config_text,
        transform=ax.transAxes,
        fontsize=9,
        verticalalignment="top",
        horizontalalignment="left",
        multialignment="left",
        bbox=dict(boxstyle="round", facecolor="whitesmoke", alpha=0.8),
        fontfamily="monospace",
    )
    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()

    plt.close(fig)

def plot_RequestSize(df, y_cols, title, ylabel, xlabel, options, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    opts = {}
    for op in options:
        op_df = df[df["T_input_size_comp"] == op]
        opts[op] = op_df

    colors = cm.tab10(np.linspace(0, 1, len(y_cols) * 2))

    for (k, v) in opts.items():
        for i, y_col in enumerate(y_cols):
            c  = colors[i * 2 + k]
            ax.plot(
                v["RequestSize"], v[y_col].clip(lower=1),
                marker="o", color=c,
                label=f"{y_col} ({"linear" if k else "no"} scaling)",
            )

    ax.set_ylabel(ylabel)
    ax.set_xlabel(xlabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    row = df.iloc[0]
    config = [
        f"Modulus Size:  1024 bits",
        f"T:  {2**int(row['T_exp'])} squarings",
        f"Size of Domain:  {10**row['private_set_digits']}",
        f"Size of Private Set:  {row['private_set_size']}",
        f"Dynamic Scaling of T:\n  - no scaling based on private set",
    ]
    ax.text(
        0.02, 0.98, "\n".join(config),
        transform=ax.transAxes,
        fontsize=9,
        verticalalignment="top",
        horizontalalignment="left",
        multialignment="left",
        bbox=dict(boxstyle="round", facecolor="whitesmoke", alpha=0.8),
        fontfamily="monospace",
    )

    fig.tight_layout()
    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()
    plt.close(fig)


def plot_cycles_variance(df, Ts, title, xlim, xlog, window, save_path=None):
    fig, ax = plt.subplots(figsize=(12, 6))

    ts = sorted(df[df["T"].isin(Ts)]["T"].unique(), reverse=True)
    colors = cm.tab10(np.linspace(0, 1, len(ts)))

    for t, color in zip(ts, colors):
        sub = df[df["T"] == t].sort_values("Iteration").reset_index(drop=True)
        cycles = sub["Cycles"]
        iters  = sub["Iteration"]

        # rolling band
        rolling_min    = cycles.rolling(window, center=True, min_periods=1).min()
        rolling_median = cycles.rolling(window, center=True, min_periods=1).median()
        rolling_max    = cycles.rolling(window, center=True, min_periods=1).max()

        ax.fill_between(iters, rolling_min, rolling_max,
                        color=color, alpha=0.15)
        ax.plot(iters, rolling_median,
                color=color, linewidth=1.5, label=f"T=2^{int(math.log2(t))}")
        
    steady = df[(df["Iteration"] >= 5) & (df["T"].isin(Ts))]["Cycles"]
    ax.axhline(steady.min(), color="green", linewidth=1.5, linestyle="--", alpha=1)

    ax.set_yticks(list(ax.get_yticks()) + [steady.min()])

    ax.set_xlabel("Iteration")
    if xlog:
        ax.set_xscale("log", base=2)
    ax.set_xlim(left=xlim)
    ax.set_ylabel("Cycles")
    ax.set_ylim(bottom=0)
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.4)
    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()
    plt.close(fig)

def plot_cycles_boxplot(df, title, Ts, save_path=None):
    fig, ax = plt.subplots(figsize=(12, 6))

    ts = sorted(df[df["T"].isin(Ts)]["T"].unique())
    data = [df[(df["T"] == t) & (df["Iteration"] > 5)]["Cycles"].values for t in ts]
    bp = ax.boxplot(data, labels=[str(t) for t in ts],
                    patch_artist=True,
                    whis=(5, 95),
                    showfliers=True,
                    flierprops=dict(marker=".", markersize=3, alpha=0.4))

    colors = cm.tab10(np.linspace(0, 1, len(ts)))
    for patch, color in zip(bp["boxes"], colors):
        patch.set_facecolor(color)
        patch.set_alpha(0.6)
    
    for median in bp["medians"]:
        median.set_color("black") 
        median.set_linewidth(1.5)

    steady = df[(df["Iteration"] >= 5) & (df["T"].isin(Ts))]["Cycles"]
    ax.axhline(steady.min(), color="green", linewidth=1.5, linestyle="--", alpha=1)

    ax.set_yticks(list(ax.get_yticks()) + [steady.min()])

    ax.set_xlabel("T (squarings)")
    ax.set_ylabel("Cycles")
    ax.set_title(title)
    ax.grid(True, linestyle="--", alpha=0.4, axis="y")
    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()
    plt.close(fig)

def plot_implementations(data, title, save_path=None):
    fig, ax = plt.subplots(figsize=(12, 6))

    colors = cm.tab10(np.linspace(0, 1, len(data)))

    for (df, label), color in zip(data, colors):
        grouped = df.groupby("T")["Cycles"].sum()
        ax.plot(grouped.index, grouped.values,
                marker="o", label=label, color=color)

    ax.set_xlabel("T (squarings)")
    ax.set_ylabel("Cycles (total)")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.4)
    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()
    plt.close(fig)
    
if __name__ == "__main__":
    gather_data()

    df_T = pd.read_csv(f"{DATA_DIR}/vary_T_exp.csv")
    df_T.columns = df_T.columns.str.strip()

    plot_T(
        df_T,
        y_cols=["SolveTime"],
        title="Solve Time vs. T",
        ylabel="Time (ms)",
        xlabel="T (squarings)",
        logy=False,
        save_path=f"{FIG_DIR}/solve_time_vs_T.png"
    )
    plot_T(
        df_T,
        y_cols=["RequestTime","SubmitTime","SolveTime"],
        title="All Times vs. T",
        ylabel="Time (ms)",
        xlabel="T (squarings)",
        logy=False,
        save_path=f"{FIG_DIR}/all_times_vs_T.png"
    )
    plot_T(
        df_T,
        y_cols=["RequestTime","SubmitTime","SolveTime"],
        title="All Times vs. T",
        ylabel="Time (ms)",
        xlabel="T (squarings)",
        logy=True,
        save_path=f"{FIG_DIR}/all_times_vs_T_log.png"
    )

    df_RS = pd.read_csv(f"{DATA_DIR}/vary_input_size.csv")
    df_RS.columns = df_RS.columns.str.strip()
    plot_RequestSize(
        df_RS,
        y_cols=["SolveTime"],
        title="Solve Time vs. Input Size",
        ylabel="Time (ms)",
        xlabel="Input Size (# elements)",
        options=[0,1],
        save_path=f"{FIG_DIR}/solve_time_vs_reqsize.png"
    )
    plot_RequestSize(
        df_RS,
        y_cols=["RequestTime","SubmitTime","SolveTime"],
        title="All Times vs. Input Size",
        ylabel="Time (ms)",
        xlabel="Input Size (# elements)",
        options=[1],
        save_path=f"{FIG_DIR}/all_times_vs_reqsize.png"
    )

    df_C = pd.read_csv(f"{DATA_DIR}/cycles.csv.cycles.csv")
    df_C.columns = df_C.columns.str.strip()
    cap = df_C.groupby("T")["Cycles"].transform(lambda x: x.quantile(0.99))
    df_C["Cycles"] = df_C["Cycles"].clip(upper=cap)

    plot_cycles_variance(
        df_C,
        Ts=[2**i for i in range(9,20, 3)],
        title="Cycles per Iteration by T",
        xlim=0,
        window=3,
        xlog=True,
        save_path=f"{FIG_DIR}/cycles_by_iteration_all_t.png"
    )

    plot_cycles_boxplot(
        df_C,
        title="Cycle Count Distribution by T",
        Ts=[2**i for i in range(9,20)],
        save_path=f"{FIG_DIR}/cycles_box_plot.png"
    )

    df_Cpy = pd.read_csv(f"{DATA_DIR}/cycles_py.csv")
    df_RS.columns = df_RS.columns.str.strip()

    df_Cpy2 = pd.read_csv(f"{DATA_DIR}/cycles_py_2.csv")
    df_RS.columns = df_RS.columns.str.strip()

    plot_implementations(
        data=[(df_C, "mbedtls squaring loop"),(df_Cpy, "python pow(x,2**T,N)")],
        title="Implementation Comparison of Puzzle Computation",
        save_path=f"{FIG_DIR}/cycles_impl_comp.png"
    )

    plot_implementations(
        data=[(df_Cpy2, "python puzzle computation")],
        title="Total Cycles vs. T",
        save_path=f"{FIG_DIR}/cycles_python.png"
    )