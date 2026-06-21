import os
import subprocess

BINARY = "./app"
DATA_DIR = "evaluation/data"
FIG_DIR = "evaluation/figures"

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

    print("running: ", args)

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
    cycles_per_it = 300000

    # experiment 1: vary T_exp, single element request
    out = f"{DATA_DIR}/vary_T_exp.csv"
    if os.path.exists(out):
        os.remove(out)
    for T_exp in range(1, 25):
        run_once(
            elements=[42],
            log=2,
            log_file="vary_T_exp",
            T_exp=T_exp,
            T_baseline_comp=0,
            T_input_size_comp=0,
            T_private_set_comp=0,
            private_set_size=100,
        )
        print(f"vary_T_exp: T_exp={T_exp} done")

    # experiment 2: vary input size, constant T vs linear T
    out = f"{DATA_DIR}/vary_input_size.csv"
    if os.path.exists(out):
        os.remove(out)
    for T_isc in [0, 1]:
        for n in range(1, 100, 10):
            elements = list(range(n))
            run_once(
                elements=elements,
                log=2,
                log_file="vary_input_size",
                T_exp=10,
                T_baseline_comp=0,
                T_input_size_comp=T_isc,
                T_private_set_comp=0,
                private_set_size=100,
            )
        print(f"vary_input_size: T_isc={T_isc} n={n} done")

    # experiment 3: cycle measurements
    out = f"{DATA_DIR}/cycles.csv.cycles.csv"
    if os.path.exists(out):
        os.remove(out)
    for t in range(5, 20):
        run_once(
            elements=[42],
            log=3,
            log_file="cycles",
            T_exp=t,
            T_baseline_comp=0,
            T_input_size_comp=0,
            T_private_set_comp=0,
            private_set_size=100,
        )
    print("cycles done")

    # experiment 4: vary expected cycles
    out = f"{DATA_DIR}/vary_exp_cycles.csv"
    if os.path.exists(out):
        os.remove(out)
    for i in range(1, 20):
        run_once(
            elements=[42],
            log=2,
            log_file="vary_exp_cycles",
            T_exp=1,
            T_baseline_comp=1,
            T_input_size_comp=0,
            T_private_set_comp=0,
            private_set_size=100,
            min_expected_cycles=(2**i)*cycles_per_it
        )

    # experiment 5: vary domain size for expected cycles comp
    out = f"{DATA_DIR}/vary_domain_size.csv"
    if os.path.exists(out):
        os.remove(out)
    for i in range(4, 20):
        run_once(
            elements=[42],
            log=2,
            log_file="vary_domain_size",
            T_exp=1,
            T_baseline_comp=1,
            T_input_size_comp=0,
            T_private_set_comp=0,
            private_set_size=100,
            min_expected_cycles=2**8 * cycles_per_it,
            private_set_digits=i
        )
    
    # experiment 6: vary private set size for expected cycles comp
    out = f"{DATA_DIR}/vary_priv_set_size.csv"
    if os.path.exists(out):
        os.remove(out)
    for i in range(100, 10001, 500):
        run_once(
            elements=[42],
            log=2,
            log_file="vary_priv_set_size",
            T_exp=1,
            T_baseline_comp=1,
            T_input_size_comp=0,
            T_private_set_comp=1,
            private_set_size=i,
            min_expected_cycles=2**8 * cycles_per_it,
            private_set_digits=4
        )

if __name__ == "__main__":
    gather_data()