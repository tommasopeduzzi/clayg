#!/bin/bash
#SBATCH --job-name=clayg_array
#SBATCH --output=stdout/slurm-%x-%A_%a.out
#SBATCH --error=stderr/slurm-%x-%A_%a.err
#SBATCH --time=28:00:00
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --array=0-5 # Set to number of lines in params.txt minus 1

CWD="$(pwd)"
CLAYG_EXEC="$CWD/../build/clayg"
PARAM_FILE="$CWD/params.txt"

LINE=$(sed -n "$((SLURM_ARRAY_TASK_ID + 1))p" "$PARAM_FILE")
read -r DECODERS DIST RUNS BASE_DIR P_START P_END P_STEP IDL_START IDL_END IDL_STEP <<< "$LINE"
echo "$LINE"

timestamp=$(date +%F_%H-%M-%S)
OUTPUT_DIR="$CWD/../$BASE_DIR/${timestamp}_D${DIST}"
mkdir -p "$OUTPUT_DIR"

echo "[$SLURM_ARRAY_TASK_ID] D=$DIST, (prob $P_START→$P_END $P_STEP) (idling $IDL_START→$IDL_END $IDL_STEP) -> $OUTPUT_DIR"

"$CLAYG_EXEC" "$DIST" "$DIST" "$DECODERS" "$OUTPUT_DIR" \
    --p_start "${P_START}" --p_end "${P_END}" --p_step "${P_STEP}" \
    --idling_time_constant_start "$IDL_START" --idling_time_constant_end "$IDL_END" --idling_time_constant_step "$IDL_STEP" \
    --dump false --runs "$RUNS"