#!/bin/bash

SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
SPEC_TRACE_ROOT=/home/phw/sim/traces/downloads
LOG_ROOT=/home/phw/workspace/simulator/logs
BUILD_CONFIG=$SIM_ROOT/_configuration.mk
BUILD_ID=$(grep -m 1 -oP '(?<=# Build ID: ).*' "$BUILD_CONFIG")
CONSTANTS_FILE=$SIM_ROOT/.csconfig/$BUILD_ID/inc/champsim_constants.h
SIM_NUM_CPU=$(grep -oP '(?<=constexpr std::size_t NUM_CPUS = )\d+' "$CONSTANTS_FILE")

NUM_WARMUP=100000000    # 1B
NUM_SIMUL=200000000     # 2B
EXP_TIME=$(date "+%y%m%d%H%M")

if [ "$#" -lt 1 ]; then
    echo "usage: run_all_combinations.sh <path_to_best_combination.dat>"
    exit 1
fi

COMB_FILE=$1

if [ ! -f "$COMB_FILE" ]; then
    echo "File $COMB_FILE does not exist."
    exit 1
fi

# Export necessary variables for the parallel execution
export SIM_ROOT SPEC_TRACE_ROOT LOG_ROOT BUILD_CONFIG BUILD_ID CONSTANTS_FILE SIM_NUM_CPU NUM_WARMUP NUM_SIMUL EXP_TIME

# Function to run simulation for a given combination
run_simulation() {
    TRACE=$1
    NUM_CPU=$(echo "$TRACE" | awk -F',' '{print NF}')
    IFS=',' read -ra TRACE_ARRAY <<< "$TRACE"

    if [ "$NUM_CPU" -ne "$SIM_NUM_CPU" ]; then
        echo "The number of provided traces ($NUM_CPU) does not match the number of CPUs in the simulator ($SIM_NUM_CPU)."
        exit 1
    fi

    CONCATED_TRACE=""
    for trace in "${TRACE_ARRAY[@]}"; do
        CONCATED_TRACE+="$SPEC_TRACE_ROOT/$trace "
    done

    TRACE_HASH=$(echo -n "$TRACE" | md5sum | awk '{print $1}')
    LOG_FILE="$LOG_ROOT/combination_${TRACE_HASH}_${EXP_TIME}.log"
    echo "BUILD_ID: $BUILD_ID" > "$LOG_FILE"
    $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $CONCATED_TRACE >> "$LOG_FILE" 2>&1
}

export -f run_simulation

# Read the combinations and run them in parallel with a maximum of 32 jobs at once
cat "$COMB_FILE" | parallel -j 32 run_simulation {}
