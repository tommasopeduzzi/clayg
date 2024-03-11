# This program is used to generate data

import os
import subprocess
import time

ds = [5, 13]
p_start = 0.00
p_end = 0.12
p_step = 0.005

# create new build
os.system("cmake --build cmake-build-debug --target clayg -j 6")

# create data folder with current time stamp
data_folder = f"data_{time.strftime('%Y-%m-%d_%H-%M-%S')}"
os.system(f"mkdir -p {data_folder}")
error_file = open(f"{data_folder}/cerr", "w+")

for decoder in ["clayg", "unionfind", "none"]:
    for d in ds:
        # split into multiple jobs, where the batches of lower probabilities are bigger
        p = p_start
        batch = 0
        while p < p_end:
            batch_size = 20 if p < 0.05 else 10 if p < 0.8 else 5
            command = ["cmake-build-debug/clayg", str(d), str(d), str(p), str(p + p_step * batch_size), str(p_step), decoder]

            print(" ".join(command))
            
            with open(f"{data_folder}/{d}_{decoder}_{p}.txt", "w+") as stdout:
                process = subprocess.Popen(command, stdout=stdout, stderr=error_file, stdin=subprocess.PIPE, shell=False)

            p += p_step * (batch_size + 1)
            batch += 1

process.wait()