import argparse
import ast
import itertools
import json
import os

# --- CLI arguments ---
parser = argparse.ArgumentParser(description="Generate params.txt and update SLURM array job script.")
parser.add_argument("config_file", help="Path to configuration file (e.g. run_config.txt)")
parser.add_argument("--param_file", default="params.txt", help="Path to output parameter file")
parser.add_argument("--batch_file", default="submit_clayg_array_job.sh", help="Path to SLURM batch file")
args = parser.parse_args()

CONFIG_FILE = args.config_file
PARAM_FILE = args.param_file
BATCH_FILE = args.batch_file

# --- Read configuration file ---
with open(CONFIG_FILE, "r") as f:
    config = json.load(f)

print(config)

# --- Helpers ---
def parse_list(val, cast=str):
    return [cast(x.strip()) for x in val.split(",")]

def parse_prob_step(p_step):
    # Normalize step format: "+0.005" or "*1.3" or "/1.3"
    if not p_step:
        return "+0.005"
    if p_step[0] in "+*/":
        return p_step
    if "." in p_step:
        return "+" + p_step
    return p_step

def generate_decoder_string(dec_dict):
    if not isinstance(dec_dict, dict):
        raise ValueError("Decoders must be a dictionary.")

    all_configs = []
    for decoder_name, params in dec_dict.items():
        if not params:  # no parameters
            all_configs.append(f"{decoder_name}()")
            continue

        keys, values = zip(*params.items())
        for combo in itertools.product(*values):
            param_str = ",".join(f"{k}={v}" for k, v in zip(keys, combo))
            all_configs.append(f"{decoder_name}({param_str})")

    return ",".join(all_configs)

# --- Extract configuration ---
distances = config.get("D", [6,8,10,12,14,16])
decoders = generate_decoder_string(config.get("Decoders", {"clayg": {}, "sl_clayg": {}, "uf": {}}))
category = config.get("Category", "new")

mode = config.get("Mode", "").lower()
probabilities_str = config.get("Probabilities", "")
if not mode:
    # Automatically detect mode
    mode = "steps" if probabilities_str else "results"

probabilities = probabilities_str if probabilities_str else [0.001, 0.005, 0.01]
p_start = float(config.get("P Start", "0.035"))
p_end = float(config.get("P End", "0.0001"))
p_step = parse_prob_step(config.get("P Step", "/1.3"))

idling_start = config.get("Idling Time Constant Start", "0.005")
idling_end = config.get("Idling Time Constant End", "0.01")
idling_step = config.get("Idling Time Constant Step", "+0.005")

runs = int(config.get("Runs", "100000"))
dump = config.get("Dump", "false")
base_dir = config.get("Results Directory", "data/results_new")

# --- Generate params lines ---
lines = []
if mode == "steps":
    directory = f"steps_{category}"
    for d in distances:
        for p in probabilities:
            lines.append(
                f"{decoders} {d} {runs} {base_dir}/{directory} {p} {p} *1.2 "
                f"{idling_start} {idling_end} {idling_step}\n"
            )
else:  # results mode
    directory = f"results_{category}"
    for d in distances:
        lines.append(
            f"{decoders} {d} {runs} {base_dir}/{directory} {p_start} {p_end} {p_step} "
            f"{idling_start} {idling_end} {idling_step}\n"
        )

# --- Write params.txt ---
with open(PARAM_FILE, "w") as f:
    f.writelines(lines)

# --- Update SLURM array size ---
num_lines = len(lines)
array_line = f"#SBATCH --array=0-{num_lines-1} # Set to number of lines in params.txt minus 1"
param_file_line = f"PARAM_FILE=\"$CWD/{PARAM_FILE}  # Path to the parameter file"

with open(BATCH_FILE, "r") as f:
    bash_lines = f.readlines()

for i, l in enumerate(bash_lines):
    if l.strip().startswith("#SBATCH --array="):
        bash_lines[i] = array_line + "\n"
        break
    if l.strip().startswith("PARAM_FILE="):
        bash_lines[i] = param_file_line + "\n"
        break

with open(BATCH_FILE, "w") as f:
    f.writelines(bash_lines)

print(f"Generated {PARAM_FILE} with {num_lines} lines.")
print(f"Updated {BATCH_FILE} with array range 0-{num_lines-1}.")