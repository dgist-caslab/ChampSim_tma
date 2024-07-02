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
TRACE=""
CONCATED_TRACE=""
EXP_TIME=`date "+%y%m%d%H%M"`

if [ "$#" -lt 2 ]; then
        echo enter target trace
        echo "usage: run_sim.sh <trace_name> <num_cpu> [NUM_WARMUP;1B] [NUM_SIMUL;5B]"
        exit
fi
TRACE=$1
NUM_CPU=$2
for (( i=0; i<$NUM_CPU; i++ )); do
        CONCATED_TRACE+="$SPEC_TRACE_ROOT/$TRACE "
done
if [ -n "$3" ]; then
        NUM_WARMUP=$2
fi
if [ -n "$4" ]; then
        NUM_SIMUL=$3
fi
if [ "$NUM_CPU" -ne "$SIM_NUM_CPU" ]; then
        echo "target number of trace(${NUM_CPU}) is not queal to the number of CPUs in the simulator(${SIM_NUM_CPU})"
        echo "need re-compile simulator"
        exit
fi

# execution command
echo "BUILD_ID: $BUILD_ID" > $LOG_ROOT/${TRACE}_${EXP_TIME}.log
$SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $CONCATED_TRACE > $LOG_ROOT/${TRACE}_${EXP_TIME}.log 2>&1