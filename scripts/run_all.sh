#!/bin/bash

SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
SPEC_TRACE_ROOT=/home/phw/sim/traces/downloads
LOG_ROOT=/home/phw/workspace/simulator/logs
# NUM_WARMUP=100000000    # 1B
# NUM_SIMUL=200000000     # 2B
NUM_WARMUP=10000000    # 100M
NUM_SIMUL=25000000     # 250M
EXE_CLUSTER=32
EXP_TIME=$(date "+%y%m%d%H%M")

run_simulator() {
    local trace_file=$1
    local trace_name=$(basename $trace_file .champsimtrace.xz)
    $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file > $LOG_ROOT/${trace_name}_${EXP_TIME}.log 2>&1
}

export -f run_simulator
export SIM_ROOT NUM_WARMUP NUM_SIMUL SPEC_TRACE_ROOT LOG_ROOT EXP_TIME

find $SPEC_TRACE_ROOT -type f -name '*.champsimtrace.xz' | parallel -j $EXE_CLUSTER run_simulator {}

echo "All simulations have been scheduled."
