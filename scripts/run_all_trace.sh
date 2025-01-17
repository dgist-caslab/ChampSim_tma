#!/bin/bash

SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
# SPEC_TRACE_ROOT=/home/phw/workspace/traces/Champsim/traces/downloads
SPEC_TRACE_ROOT=/home/phw/workspace/traces/Champsim/spec/
LOG_ROOT=/home/phw/workspace/simulator/logs/last_exp
# NUM_WARMUP=100000000    # 1B
# NUM_SIMUL=200000000     # 2B
NUM_WARMUP=100000000    # 100M
NUM_SIMUL=250000000     # 250M
EXE_CLUSTER=24
EXP_TIME=$(date "+%y%m%d%H%M")

run_simulator() {
    local trace_file=$1
    local trace_name=$(basename $trace_file .champsimtrace.xz)
    echo $trace_file
    echo $trace_name
    echo $SIM_ROOT/bin/champsim_$2 --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file > $LOG_ROOT/${trace_name}_${EXP_TIME}.log 2>&1
    $SIM_ROOT/bin/champsim_$2 --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file > $LOG_ROOT/${trace_name}_${EXP_TIME}.log 2>&1
}

export -f run_simulator
export SIM_ROOT NUM_WARMUP NUM_SIMUL SPEC_TRACE_ROOT LOG_ROOT EXP_TIME

# for setup in no_pref pref_d2 pref_d4 pref_d8 pref_d16 pref_dyn;
# for setup in berti;
# for setup in bingo berti;
# for setup in cap;
for setup in dyn;
# for setup in bingo;
# for setup in berti;
# for setup in no dyn bingo berti cap;
do
    mkdir $LOG_ROOT/../tma_$setup
    find $SPEC_TRACE_ROOT -type f -name '*.champsimtrace.xz' | parallel -j $EXE_CLUSTER run_simulator {} $setup
    echo "All simulations have been scheduled($setup)."
    mv $LOG_ROOT/* $LOG_ROOT/../tma_$setup
    sleep 10
done
