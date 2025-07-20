from datetime import datetime
import os
import re
import subprocess


cwd = os.getcwd()
base_dir = os.path.join(cwd, "data/new_average_operations")
os.makedirs(base_dir, exist_ok=True)

decoders = ["sl_clayg", "clayg", "uf"]

distances = [10, 12, 14, 16, 18]

probabilities = [0.01, 0.005]
start = 0.0001
end = 0.05
step = 1.2
use_probability_list = True

runs = 200000

existing_ids = [
    int(match.group(1))
    for f in os.listdir(base_dir)
    if os.path.isdir(os.path.join(base_dir, f))
    and (match := re.match(r"(\d+)-", f))
]
next_id = max(existing_ids, default=0) + 1
if next_id not in existing_ids:
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    output_dir = os.path.join(base_dir, f"{next_id}-{timestamp}")
    os.makedirs(output_dir, exist_ok=True)
else:
    output_dir = next(
        os.path.join(base_dir, f)
        for f in os.listdir(base_dir)
        if f.startswith(str(next_id))
    )

if use_probability_list:
    for d in distances:
        for p in probabilities:
            exit(1)
            command = f"/home/tommaso-peduzzi/Documents/clayg/cmake-build-debug/clayg {d} {d} {p} {p*1.5} {','.join(decoders)} {output_dir} p_step=*2 dump=false runs={runs}; sleep 1; exit" # type: ignore
            subprocess.Popen(["xterm", "-e", f'cd "{cwd}" && {command}; bash'])
else:
    for d in distances:
        exit(1)
        command = f"/home/tommaso-peduzzi/Documents/clayg/cmake-build-debug/clayg {d} {d} {start} {end} {','.join(decoders)} {output_dir} p_step=*{step} dump=false runs={runs}; sleep 1; exit" # type: ignore
        subprocess.Popen(["xterm", "-e", f'cd "{cwd}" && {command}; bash'])


