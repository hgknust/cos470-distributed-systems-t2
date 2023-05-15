import subprocess as sp
import pandas as pd
import numpy as np


# Params
n_range = [int(x) for x in [1e7, 1e8, 1e9]]
k_range = [1, 2, 4, 8, 16]
repetitions = 10

# Create DF
df = pd.DataFrame({"N": [], "K": [], "time (s)": []})

for n in n_range:
    for k in k_range:

        print(f"Starting tests with N={n}, K={k}.\n")
        times = []
        for _ in range(repetitions):

            try:
                out = sp.check_output(["./sum.out", str(n), str(k)])
            except sp.CalledProcessError:
                print(f"Failed execution for N={n}, K={k}. Ignoring..")
                continue

            # Parse timing out of the output
            out = str(out)[2:-1]
            start = out.find("CPU time spent: ")
            time = float(out[start + 16: start + 24])

            times.append(time)

        df = df.append({"N": n, "K": k, "time (s)": np.array(times).mean()}, ignore_index=True)

df.to_csv("results.csv")
