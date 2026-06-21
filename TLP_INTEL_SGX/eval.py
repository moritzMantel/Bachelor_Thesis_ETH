import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np
import math
import csv

labels = {
    "RequestTime": "Puzzle Generation",
    "SolveTime": "Puzzle Solver",
    "SubmitTime": "Puzzle Verification",
    "SetConfigTime": "Configuration Change",
    "InitTime": "Crypto Initialisation",
    "TeardownTime": "Crypto Freeing"
}
    
def plot_T(df, y_cols, title, ylabel, xlabel, logy, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    df["T"] = 2 ** df["T_exp"]
    colors = cm.tab10(np.linspace(0, 1, len(y_cols)))
    for y_col, color in zip(y_cols, colors):
        y = df[y_col].clip(lower=1)
        ax.plot(df["T"], y, marker="o", label=labels[y_col], color=color)

    if logy:
        ax.set_yscale("log", base=2)
    ax.set_ylabel(ylabel)


    ax.set_xscale("log", base=2)
    ax.set_xlim(left=1)
    ax.set_xlabel(xlabel)

    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)
    
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
                label=f"{labels[y_col]} ({"linear" if k else "no"} scaling)",
            )

    ax.set_ylabel(ylabel)
    ax.set_xlabel(xlabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

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
    
def plot_exp_cycles(df, y_cols, title, ylabel, xlabel, logy, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = cm.tab10(np.linspace(0, 1, len(y_cols)))
    for y_col, color in zip(y_cols, colors):
        y = df[y_col].clip(lower=1)
        ax.plot(df["min_expected_cycles"], y, marker="o", label=labels[y_col], color=color)


    ax.set_ylabel(ylabel)
    ax.set_xlabel(xlabel)

    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)

    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()

    plt.close(fig)

def plot_domain_sizes(df, y_cols, title, ylabel, xlabel, logy, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = cm.tab10(np.linspace(0, 1, len(y_cols)))
    df["DomainSize"] = 10 ** df["private_set_digits"]
    for y_col, color in zip(y_cols, colors):
        y = df[y_col].clip(lower=1)
        ax.plot(df["DomainSize"], y, marker="o", label=labels[y_col], color=color)


    ax.set_ylabel(ylabel)
    ax.set_xlabel(xlabel)
    ax.set_xscale("log", base=10)

    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)
    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()

    plt.close(fig)

def plot_private_set_size(df, y_cols, title, ylabel, xlabel, logy, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = cm.tab10(np.linspace(0, 1, len(y_cols)))
    for y_col, color in zip(y_cols, colors):
        y = df[y_col].clip(lower=1)
        ax.plot(df["private_set_size"], y, marker="o", label=labels[y_col], color=color)


    ax.set_ylabel(ylabel)
    ax.set_xlabel(xlabel)

    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()

    plt.close(fig)


def plot_encl_data(df, y_cols, title, ylabel, xlabel, logy, save_path=None):
    fig, ax = plt.subplots(figsize=(10, 6))

    colors = cm.tab10(np.linspace(0, 1, len(y_cols)))
    for y_col, color in zip(y_cols, colors):
        y = df[df["T_exp"] == 15][y_col]
        ax.plot(df[df["T_exp"] == 15]["num_bits"] * 2, y, marker="o", label=y_col, color=color)

    if logy:
        ax.set_yscale("log", base=2)
    ax.set_ylabel(ylabel)


    ax.set_xscale("log", base=2)
    ax.set_xlabel(xlabel)

    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)

    fig.tight_layout()

    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()

    plt.close(fig)

if __name__ == "__main__":
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

    df_E = pd.read_csv(f"{DATA_DIR}/vary_exp_cycles.csv")
    df_E.columns = df_E.columns.str.strip()

    plot_exp_cycles(
        df_E,
        y_cols=["RequestTime","SubmitTime","SolveTime"],
        title="All Times vs. Expected Cycles",
        ylabel="Time (ms)",
        xlabel="Expected Cycles (per request)",
        logy=False,
        save_path=f"{FIG_DIR}/all_times_vs_exp_cycles.png"
    )

    df_D = pd.read_csv(f"{DATA_DIR}/vary_domain_size.csv")
    df_D.columns = df_D.columns.str.strip()

    plot_domain_sizes(
        df_D,
        y_cols=["RequestTime","SubmitTime","SolveTime"],
        title="All Times vs. Domain Size",
        ylabel="Time (ms)",
        xlabel="Domain Size",
        logy=False,
        save_path=f"{FIG_DIR}/all_times_vs_domain_size.png"
    )

    df_S = pd.read_csv(f"{DATA_DIR}/vary_priv_set_size.csv")
    df_S.columns = df_S.columns.str.strip()

    plot_private_set_size(
        df_S,
        y_cols=["RequestTime","SubmitTime","SolveTime"],
        title="All Times vs. Private Set Size",
        ylabel="Time (ms)",
        xlabel="Private Set Size",
        logy=False,
        save_path=f"{FIG_DIR}/all_times_vs_priv_set_size.png"
    )

    df_Encl = pd.read_csv(f"{DATA_DIR}/encl_data.csv")
    df_Encl.columns = df_Encl.columns.str.strip()

    plot_encl_data(
        df_Encl,
        y_cols=["SetConfigTime","InitTime","TeardownTime"],
        title="All Enclave Times vs. Modulus Bits",
        ylabel="Time (ms)",
        xlabel="Number of Bits",
        logy=False,
        save_path=f"{FIG_DIR}/encl_times_vs_num_bits.png"
    )

    plot_T(
        df_Encl[df_Encl["num_bits"] == 1024],
        y_cols=["SetConfigTime","InitTime","TeardownTime"],
        title="All Enclave Times vs. T",
        ylabel="Time (ms)",
        xlabel="T (squarings)",
        logy=True,
        save_path=f"{FIG_DIR}/encl_times_vs_T.png"
    )