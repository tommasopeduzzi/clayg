#!/bin/bash
#SBATCH --job-name=clayg_array
#SBATCH --output=slurm-%x-%A_%a.out
#SBATCH --error=slurm-%x-%A_%a.err
#SBATCH --time=12:00:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --array=0-<N>  # Set to number of lines in params.txt minus 1

CLAYG_EXEC="/../build/clayg"
CWD="$(pwd)"
BASE_DIR="$CWD/data/new_average_operations"
PARAM_FILE="$CWD/params.txt"
DECODERS="sl_clayg,clayg,uf"
RUNS=200000

LINE=$(sed -n "$((SLURM_ARRAY_TASK_ID + 1))p" "$PARAM_FILE")
read -r DIST MODE REST <<< "$LINE"

# Generate output directory with timestamp
mkdir -p "$BASE_DIR"
next_id=$(find "$BASE_DIR" -maxdepth 1 -type d -printf "%f\n" | grep -E '^[0-9]+-' | sed 's/-.*//' | sort -n | tail -n1)
if [[ -z "$next_id" ]]; then
    next_id=1
else
    next_id=$((next_id + 1))
fi

timestamp=$(date +%F_%H-%M-%S)

if [[ "$MODE" == "prob" ]]; then
    P="$REST"
    P_END=$(echo "$P * 1.5" | bc -l)
    OUTPUT_DIR="$BASE_DIR/${next_id}-${timestamp}_D${DIST}_P${P}"
    mkdir -p "$OUTPUT_DIR"
    echo "[$SLURM_ARRAY_TASK_ID] MODE=LIST D=$DIST, P=$P -> $OUTPUT_DIR"
    "$CLAYG_EXEC" "$DIST" "$DIST" "$P" "$P_END" "$DECODERS" "$OUTPUT_DIR" p_step="*2" dump=false runs=$RUNS

elif [[ "$MODE" == "range" ]]; then
    read -r P_START P_END P_STEP <<< "$REST"
    OUTPUT_DIR="$BASE_DIR/${next_id}-${timestamp}_D${DIST}_Range"
    mkdir -p "$OUTPUT_DIR"
    echo "[$SLURM_ARRAY_TASK_ID] MODE=RANGE D=$DIST, [$P_START, $P_END] step $P_STEP -> $OUTPUT_DIR"
    "$CLAYG_EXEC" "$DIST" "$DIST" "$P_START" "$P_END" "$DECODERS" "$OUTPUT_DIR" p_step="*${P_STEP}" dump=false runs=$RUNS

else
    echo "[$SLURM_ARRAY_TASK_ID] ERROR: Unknown mode '$MODE'"
    exit 1
fi