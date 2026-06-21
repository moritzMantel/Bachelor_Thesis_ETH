import math
import matplotlib.cm as cm
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

DATA_DIR = "evaluation/data"
FIG_DIR  = "evaluation/figures"

LABELS = {
    "RequestTime":  "Puzzle Generation",
    "SolveTime":    "Puzzle Solver",
    "SubmitTime":   "Puzzle Verification",
    "SetConfigTime":"SetConfigTime",
    "InitTime":     "InitTime",
    "TeardownTime": "TeardownTime",
    "mbedtls":      "mbedTLS",
    "openssl_exp":  "OpenSSL-exp",
    "openssl_loop": "OpenSSL-loop",
    "python":       "Python",
    "comp_const":   "const_time_memcmp",
    "comp_mpi":     "mbedtls_mpi_cmp_mpi",
    "openssl":      "OpenSSL"
}

IMPL_COLORS = {
    "mbedtls":      "#1f77b4",
    "openssl_exp":  "#ff7f0e",
    "openssl_loop": "#2ca02c",
    "python":       "#9467bd",
}

HARDWARE_MARKER = {
    "Apple M2":            "o",
    "Intel Core i9-9900K": "^",
}


# helpers
def _save_or_show(fig, save_path):
    if save_path:
        fig.savefig(save_path, dpi=150)
        print(f"Saved: {save_path}")
    else:
        plt.show()
    plt.close(fig)

def plot_line(series, title, xlabel, ylabel,
              logx=False, logy=False, logx_base=2, logy_base=2,
              xlim_left=None, figsize=(10, 6), save_path=None):

    fig, ax = plt.subplots(figsize=figsize)
    for s in series:
        ax.plot(
            s["x"], np.clip(s["y"], 1, None),
            marker=s.get("marker", "o"),
            markersize=s.get("markersize", 4),
            linewidth=s.get("linewidth", 1.5),
            color=s["color"],
            label=s["label"],
        )
    if logx:
        ax.set_xscale("log", base=logx_base)
    if logy:
        ax.set_yscale("log", base=logy_base)
    if xlim_left is not None:
        ax.set_xlim(left=xlim_left)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.5)
    fig.tight_layout()
    _save_or_show(fig, save_path)

def _df_series(df, x_col, y_cols, n_colors=None):
    """Build series list from a dataframe for plot_line."""
    n = n_colors or len(y_cols)
    colors = cm.tab10(np.linspace(0, 1, n))
    return [
        {"x": df[x_col], "y": df[y_col], "label": LABELS.get(y_col, y_col), "color": color}
        for y_col, color in zip(y_cols, colors)
    ]


# special plots
def plot_cycles_variance(df, Ts, title, xlim, xlog, window, save_path=None):
    fig, ax = plt.subplots(figsize=(12, 6))
    ts     = sorted(df[df["T"].isin(Ts)]["T"].unique(), reverse=True)
    colors = cm.tab10(np.linspace(0, 1, len(ts)))

    for t, color in zip(ts, colors):
        sub    = df[df["T"] == t].sort_values("Iteration").reset_index(drop=True)
        cycles = sub["Cycles"]
        iters  = sub["Iteration"]
        rolling_min    = cycles.rolling(window, center=True, min_periods=1).min()
        rolling_median = cycles.rolling(window, center=True, min_periods=1).median()
        rolling_max    = cycles.rolling(window, center=True, min_periods=1).max()
        ax.fill_between(iters, rolling_min, rolling_max, color=color, alpha=0.15)
        ax.plot(iters, rolling_median, color=color, linewidth=1.5,
                label=f"T=2^{int(math.log2(t))}")

    steady = df[(df["Iteration"] >= 5) & (df["T"].isin(Ts))]["Cycles"]
    ax.axhline(steady.min(), color="green", linewidth=1.5, linestyle="--")
    ax.set_yticks(list(ax.get_yticks()) + [steady.min()])
    if xlog:
        ax.set_xscale("log", base=2)
    ax.set_xlim(left=xlim)
    ax.set_ylim(bottom=0)
    ax.set_xlabel("Iteration")
    ax.set_ylabel("Cycles")
    ax.set_title(title)
    ax.legend()
    ax.grid(True, linestyle="--", alpha=0.4)
    fig.tight_layout()
    _save_or_show(fig, save_path)

def plot_cycles_boxplot(df, title, Ts, save_path=None):
    fig, ax = plt.subplots(figsize=(12, 6))
    ts   = sorted(df[df["T"].isin(Ts)]["T"].unique())
    data = [df[(df["T"] == t) & (df["Iteration"] > 5)]["Cycles"].values for t in ts]
    bp   = ax.boxplot(data, labels=[str(t) for t in ts],
                      patch_artist=True, whis=(5, 95), showfliers=True,
                      flierprops=dict(marker=".", markersize=3, alpha=0.4))
    colors = cm.tab10(np.linspace(0, 1, len(ts)))
    for patch, color in zip(bp["boxes"], colors):
        patch.set_facecolor(color)
        patch.set_alpha(0.6)
    for median in bp["medians"]:
        median.set_color("black")
        median.set_linewidth(1.5)
    steady = df[(df["Iteration"] >= 5) & (df["T"].isin(Ts))]["Cycles"]
    ax.axhline(steady.min(), color="green", linewidth=1.5, linestyle="--")
    ax.set_yticks(list(ax.get_yticks()) + [steady.min()])
    ax.set_xlabel("T (squarings)")
    ax.set_ylabel("Cycles")
    ax.set_title(title)
    ax.grid(True, linestyle="--", alpha=0.4, axis="y")
    fig.tight_layout()
    _save_or_show(fig, save_path)

def plot_broken_boxplot(df, title, ylim_top, ylim_bot, save_path=None):
    mbedtls = df[df["Impl"] == "mbedtls"]["AvgCycles"]
    openssl = df[df["Impl"] == "openssl"]["AvgCycles"]

    fig, (ax_top, ax_bot) = plt.subplots(
        2, 1, figsize=(8, 7), sharex=True,
        gridspec_kw={"height_ratios": [1, 1], "hspace": 0.05}
    )
    for ax in (ax_top, ax_bot):
        bp = ax.boxplot(
            [mbedtls, openssl], labels=["mbedTLS", "OpenSSL"],
            patch_artist=True, whis=(5, 95), showfliers=True,
            flierprops=dict(marker=".", markersize=4, alpha=0.5)
        )
        for patch, color in zip(bp["boxes"], ["steelblue", "tomato"]):
            patch.set_facecolor(color)
            patch.set_alpha(0.6)
        for median in bp["medians"]:
            median.set_color("black")
            median.set_linewidth(1.5)
        ax.grid(True, linestyle="--", alpha=0.4, axis="y")

    ax_top.set_ylim(*ylim_top)
    ax_bot.set_ylim(*ylim_bot)
    ax_top.spines["bottom"].set_visible(False)
    ax_bot.spines["top"].set_visible(False)
    ax_top.tick_params(bottom=False)

    d = 0.015
    for ax, corners in [(ax_top, [(-d, +d, -d, +d), (1-d, 1+d, -d, +d)]),
                         (ax_bot,  [(-d, +d, 1-d, 1+d), (1-d, 1+d, 1-d, 1+d)])]:
        kw = dict(transform=ax.transAxes, color="k", clip_on=False, linewidth=1)
        for x0, x1, y0, y1 in corners:
            ax.plot((x0, x1), (y0, y1), **kw)

    ax_bot.set_ylabel("Average Cycles per Squaring")
    ax_top.set_title(title)
    fig.tight_layout()
    _save_or_show(fig, save_path)


if __name__ == "__main__":
    TIME_COLS  = ["RequestTime", "SubmitTime", "SolveTime"]
    ENCL_COLS  = ["SetConfigTime", "InitTime", "TeardownTime"]

    # vary T
    df_T      = pd.read_csv(f"{DATA_DIR}/vary_T_exp.csv")
    df_T.columns = df_T.columns.str.strip()
    df_T["T"] = 2 ** df_T["T_exp"]

    plot_line(_df_series(df_T, "T", ["SolveTime"]),
              title="Solve Time vs. T", xlabel="T (squarings)", ylabel="Time (ms)",
              logx=True, xlim_left=1,
              save_path=f"{FIG_DIR}/solve_time_vs_T.png")

    plot_line(_df_series(df_T, "T", TIME_COLS),
              title="All Times vs. T", xlabel="T (squarings)", ylabel="Time (ms)",
              logx=True, logy=True, xlim_left=1,
              save_path=f"{FIG_DIR}/all_times_vs_T_log.png")

    # vary input size
    df_RS = pd.read_csv(f"{DATA_DIR}/vary_input_size.csv")
    df_RS.columns = df_RS.columns.str.strip()

    # vary T_input_size
    colors_rs = cm.tab10(np.linspace(0, 1, 4))
    rs_series_solve = [
        {"x": df_RS[df_RS["T_input_size_comp"] == k]["RequestSize"],
         "y": df_RS[df_RS["T_input_size_comp"] == k]["SolveTime"],
         "label": f"Puzzle Solver ({'linear' if k else 'no'} scaling)",
         "color": colors_rs[k]}
        for k in [0, 1]
    ]
    plot_line(rs_series_solve,
              title="Solve Time vs. Input Size", xlabel="Input Size (# elements)", ylabel="Time (ms)",
              save_path=f"{FIG_DIR}/solve_time_vs_reqsize.png")

    rs_series_all = [
        {"x": df_RS[df_RS["T_input_size_comp"] == 1]["RequestSize"],
         "y": df_RS[df_RS["T_input_size_comp"] == 1][col],
         "label": f"{LABELS[col]} (linear scaling)",
         "color": colors_rs[i]}
        for i, col in enumerate(TIME_COLS)
    ]
    plot_line(rs_series_all,
              title="All Times vs. Input Size", xlabel="Input Size (# elements)", ylabel="Time (ms)",
              save_path=f"{FIG_DIR}/all_times_vs_reqsize.png")

    # cycles variance
    df_C = pd.read_csv(f"{DATA_DIR}/cycles.csv.cycles.csv")
    df_C.columns = df_C.columns.str.strip()
    cap = df_C.groupby("T")["Cycles"].transform(lambda x: x.quantile(0.99))
    df_C["Cycles"] = df_C["Cycles"].clip(upper=cap)

    plot_cycles_variance(df_C, Ts=[2**i for i in range(9, 20, 3)],
                         title="Cycles per Iteration by T",
                         xlim=0, window=3, xlog=True,
                         save_path=f"{FIG_DIR}/cycles_by_iteration_all_t.png")

    # vary expected cycles
    df_E = pd.read_csv(f"{DATA_DIR}/vary_exp_cycles.csv")
    df_E.columns = df_E.columns.str.strip()

    plot_line(_df_series(df_E, "min_expected_cycles", TIME_COLS),
              title="All Times vs. Expected Cycles",
              xlabel="Expected Cycles (per request)", ylabel="Time (ms)",
              logx=True, logy=True,
              save_path=f"{FIG_DIR}/all_times_vs_exp_cycles.png")

    # vary domain size
    df_D = pd.read_csv(f"{DATA_DIR}/vary_domain_size.csv")
    df_D.columns = df_D.columns.str.strip()
    df_D["DomainSize"] = 10 ** df_D["private_set_digits"]

    plot_line(_df_series(df_D, "DomainSize", TIME_COLS),
              title="All Times vs. Domain Size", xlabel="Domain Size", ylabel="Time (ms)",
              logx=True, logx_base=10,
              save_path=f"{FIG_DIR}/all_times_vs_domain_size.png")

    # vary private set size
    df_S = pd.read_csv(f"{DATA_DIR}/vary_priv_set_size.csv")
    df_S.columns = df_S.columns.str.strip()

    plot_line(_df_series(df_S, "private_set_size", TIME_COLS),
              title="All Times vs. Private Set Size", xlabel="Private Set Size", ylabel="Time (ms)",
              save_path=f"{FIG_DIR}/all_times_vs_priv_set_size.png")

    # enclave overhead
    df_Encl = pd.read_csv(f"{DATA_DIR}/encl_data.csv")
    df_Encl.columns = df_Encl.columns.str.strip()

    df_encl_bits = df_Encl[df_Encl["T_exp"] == 15].copy()
    df_encl_bits["ModulusBits"] = df_encl_bits["num_bits"] * 2
    plot_line(_df_series(df_encl_bits, "ModulusBits", ENCL_COLS),
              title="All Enclave Times vs. Modulus Bits",
              xlabel="Number of Bits", ylabel="Time (ms)",
              logx=True,
              save_path=f"{FIG_DIR}/encl_times_vs_num_bits.png")

    df_encl_t = df_Encl[df_Encl["num_bits"] == 1024].copy()
    df_encl_t["T"] = 2 ** df_encl_t["T_exp"]
    plot_line(_df_series(df_encl_t, "T", ENCL_COLS),
              title="All Enclave Times vs. T", xlabel="T (squarings)", ylabel="Time (ms)",
              logx=True, logy=True,
              save_path=f"{FIG_DIR}/encl_times_vs_T.png")

    # implementation comparisons
    df_mac = pd.read_csv(f"{DATA_DIR}/impl_solve.csv")
    df_vm  = pd.read_csv(f"{DATA_DIR}/impl_solve_vm.csv")
    df_mac["Source"] = "Apple M2"
    df_vm["Source"]  = "Intel Core i9-9900K"
    df_impl = pd.concat([df_mac, df_vm], ignore_index=True)

    impl_series = [
        {"x": g.sort_values("T")["T"], "y": g.sort_values("T")["Time"],
         "label": f"{LABELS.get(impl, impl)} ({source})",
         "color": IMPL_COLORS.get(impl, "gray"),
         "marker": HARDWARE_MARKER[source]}
        for (source, impl), g in df_impl.groupby(["Source", "Impl"])
    ]
    plot_line(impl_series,
              title="Implementation Comparison of Puzzle Computation on Different Hardware",
              xlabel="T (squarings)", ylabel="Time (ms)",
              logx=True, logy=True,
              save_path=f"{FIG_DIR}/impl_comparison.png")

    # implementation comparison (total cycles)
    df_tc = pd.read_csv(f"{DATA_DIR}/impl_cycles_total.csv")
    df_tc_series = [
        {"x": g["T"], "y": g["TotalCycles"],
         "label": LABELS.get(impl, impl),
         "color": IMPL_COLORS.get(impl, "gray")}
        for impl, g in df_tc.groupby("Impl")
    ]
    plot_line(df_tc_series,
              title="Implementation Comparison of Puzzle Computation in Cycles",
              xlabel="T (squarings)", ylabel="Total Cycles",
              logx=True, logy=True,
              save_path=f"{FIG_DIR}/impl_comparison_cycles.png")

    # mbedTLS vs OpenSSL avg cycles (box plot)
    df_ac = pd.read_csv(f"{DATA_DIR}/impl_cycles.csv")
    df_ac = df_ac[df_ac["T"] >= 2**8]
    plot_broken_boxplot(df_ac,
                        title="Cycles per Squaring: mbedTLS vs OpenSSL\n(aggregated over T from [2^5, 2^24])",
                        ylim_top=(320000, 340000),
                        ylim_bot=(19000, 21000),
                        save_path=f"{FIG_DIR}/cycles_boxplot.png")

    # side channel: comparison timing
    df_sc = pd.read_csv(f"{DATA_DIR}/sidechannel.csv")
    df_sc["Time"] = df_sc["Time"] / 1000
    sc_series = [
        {"x": g.sort_values("MatchingBytes")["MatchingBytes"],
         "y": g.sort_values("MatchingBytes")["Time"],
         "label": LABELS.get(impl, impl),
         "color": color, "markersize": 3, "linewidth": 1}
        for (impl, g), color in zip(df_sc.groupby("Impl"),
                                     cm.tab10(np.linspace(0, 1, df_sc["Impl"].nunique())))
    ]
    plot_line(sc_series,
              title="Side-Channel: Comparison Timing vs. Matching Bytes",
              xlabel="Matching Bytes (prefix length)", ylabel="Time (ns)",
              save_path=f"{FIG_DIR}/sidechannel_comparison.png")

    # side channel: modular exponentiation
    df_se = pd.read_csv(f"{DATA_DIR}/sidechannel_exp.csv")
    df_se["Time"] = df_se["Time"] / 1000
    plot_line([{"x": df_se["E"], "y": df_se["Time"],
                "label": "mbedtls_mpi_exp_mod", "color": "blue",
                "markersize": 3, "linewidth": 1}],
              title="Side-Channel: Modular Exponentiation Time vs. Exponent Size",
              xlabel="Exponent", ylabel="Time (ns)",
              logx=True,
              save_path=f"{FIG_DIR}/sidechannel_exp.png")