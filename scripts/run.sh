#!/bin/bash
SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
SPEC_TRACE_ROOT=/home/phw/sim/traces/downloads
LOG_ROOT=/home/phw/workspace/simulator/logs

NUM_WARMUP=100000000    # 1B
NUM_SIMUL=200000000     # 2B
TRACE=""
EXP_TIME=`date "+%y%m%d%H%M"`

if [ -z "$1" ]; then
        echo enter target trace
        echo "usage: run_sim.sh <trace_name> [NUM_WARMUP;1B] [NUM_SIMUL;5B]"
        exit
else
        TRACE=$1
fi
if [ -n "$2" ]; then
        NUM_WARMUP=$2
fi
if [ -n "$3" ]; then
        NUM_SIMUL=$3
fi

# execution command
$SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $SPEC_TRACE_ROOT/$TRACE $SPEC_TRACE_ROOT/$TRACE > $LOG_ROOT/${TRACE}_${EXP_TIME}.log 2>&1


