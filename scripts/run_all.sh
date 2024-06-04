#!/bin/bash
SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
SPEC_TRACE_ROOT=/home/phw/sim/traces/downloads
LOG_ROOT=/home/phw/workspace/simulator/logs
CUR_DIR=`pwd`

NUM_WARMUP=100000000    # 1B
NUM_SIMUL=200000000     # 5B
EXE_CLUSTER=16
TRACE=""
EXP_TIME=`date "+%y%m%d%H%M"`

declare -A max_files

sorted_traces=()

# Loop through each file in the current directory
cd $SPEC_TRACE_ROOT
for file in *.champsimtrace.xz; do
  # Extract the identifier and the B value from the file name
  identifier=$(echo "$file" | awk -F '-' '{print $1}')
  b_value=$(echo "$file" | awk -F '-' '{print $2}' | sed 's/B.champsimtrace.xz//')

  # Check if the identifier is already in the associative array
  if [[ -z "${max_files[$identifier]}" || $b_value -gt $(echo "${max_files[$identifier]}" | awk -F '-' '{print $2}' | sed 's/B.champsimtrace.xz//') ]]; then
    max_files[$identifier]="$file"
  fi
done

for key in $(echo "${!max_files[@]}" | tr ' ' '\n' | sort); do
  sorted_traces+=("${max_files[$key]}")
done

cd $CUR_DIR

# if [ -z "$1" ]; then
#     echo enter target trace
#     echo "usage: run_all.sh [NUM_WARMUP;1B] [NUM_SIMUL;2B]"
#     exit
# fi
if [ -n "$1" ]; then
    NUM_WARMUP=$1
fi
if [ -n "$2" ]; then
    NUM_SIMUL=$2
fi

for trace in "${sorted_traces[@]}"; do
    echo "$trace"
    $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $SPEC_TRACE_ROOT/$trace > $LOG_ROOT/${trace}_${EXP_TIME}.log 2>&1 &
done

wait

echo "All simulations have completed."

