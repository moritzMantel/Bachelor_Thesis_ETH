import pandas as pd
import matplotlib.pyplot as plt

def load_data(path):
    df = pd.read_csv(path)
    return df

def filter_data(df, **conditions):
    filtered = df.copy()
    for col, val in conditions.items():
        filtered = filtered[filtered[col] == val]
    return filtered

def plot_metric(df, x_col, y_col, title=None):
    plt.figure()
    plt.plot(df[x_col], df[y_col], marker='o')
    plt.xlabel(x_col)
    plt.ylabel(y_col)
    if title:
        plt.title(title)
    plt.grid()
    plt.savefig("plot.png", dpi=300)

def plot_grouped(df, x_col, y_col, group_col):
    plt.figure()
    
    for key, group in df.groupby(group_col):
        plt.plot(group[x_col], group[y_col], marker='o', label=f"{group_col}={key}")
    
    plt.xlabel(x_col)
    plt.ylabel(y_col)
    plt.legend()
    plt.grid()
    plt.savefig("plot.png", dpi=300)


if __name__ == "__main__":
    df = load_data("config.csv")
    df.columns = df.columns.str.strip()
    subset = filter_data(df,
        T_input_size_comp=0,
        T_baseline_comp=0,
        T_private_set_comp=0,
        RequestSize=1
    )
    plot_metric(subset, "T_exp", "SolveTime",
            title="SolveTime vs RequestSize")
