#!/bin/bash

# Define the root paths
SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/410.bwaves-2097B.champsimtrace.xz  # Sample trace file path
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/603.bwaves_s-1740B.champsimtrace.xz  # Sample trace file path
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/602.gcc_s-2375B.champsimtrace.xz  # Sample trace file path
# TRACE_FEED=/home/phw/workspace/scripts/about_trace/parsed_bingo_bits/602.gcc_s-2375B_2411291532.csv
# TRACE_FILE_A=/home/phw/workspace/traces/Champsim/spec/602.gcc_s-2375B.champsimtrace.xz  # Sample trace file path
# TRACE_FEED_A=/home/phw/workspace/scripts/about_trace/parsed_bingo_bits/602.gcc_s-2375B_2411291532.csv
# TRACE_FILE_B=/home/phw/workspace/traces/Champsim/spec/607.cactuBSSN_s-2421B.champsimtrace.xz  # Sample trace file path
# TRACE_FEED_B=/home/phw/workspace/scripts/about_trace/parsed_bingo_bits/607.cactuBSSN_s-2421B_2411291532.csv
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/607.cactuBSSN_s-2421B.champsimtrace.xz  # Sample trace file path
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/649.fotonik3d_s-10881B.champsimtrace.xz  # Sample trace file path
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/602.gcc_s-2375B.champsimtrace.xz
# LOG_FILE=/home/phw/workspace/simulator/logs/last_exp/sim_test.log

TRACE_FILE_A=/home/phw/workspace/traces/Champsim/spec/649.fotonik3d_s-7084B.champsimtrace.xz
TRACE_FILE_B=/home/phw/workspace/traces/Champsim/spec/470.lbm-1274B.champsimtrace.xz
TRACE_FILE_C=/home/phw/workspace/traces/Champsim/spec/603.bwaves_s-2931B.champsimtrace.xz
TRACE_FILE_D=/home/phw/workspace/traces/Champsim/spec/605.mcf_s-1554B.champsimtrace.xz
TRACE_FILE_E=/home/phw/workspace/traces/Champsim/spec/649.fotonik3d_s-10881B.champsimtrace.xz
TRACE_FILE_F=/home/phw/workspace/traces/Champsim/spec/433.milc-274B.champsimtrace.xz
TRACE_FILE_G=/home/phw/workspace/traces/Champsim/spec/410.bwaves-1963B.champsimtrace.xz
TRACE_FILE_H=/home/phw/workspace/traces/Champsim/spec/603.bwaves_s-2609B.champsimtrace.xz

# Define the warmup and simulation instructions
NUM_WARMUP=31250000  # 250M instructions for warmup
NUM_SIMUL=125000000   # 125M instructions for simulation

# Run the simulator with a sample trace
run_simulator() {
    local trace_file_a=$1
    local trace_file_b=$2
    local trace_file_c=$3
    local trace_file_d=$4
    local trace_file_e=$5
    local trace_file_f=$6
    local trace_file_g=$7
    local trace_file_h=$8

    echo "Running simulator with trace: $trace_file_a $trace_file_b $trace_file_c $trace_file_d $trace_file_e $trace_file_f $trace_file_g $trace_file_h"
    # echo "$SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b --feeds $trace_feed_a $trace_feed_b"

    # Execute the simulator and log the output
    $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b $trace_file_c $trace_file_d $trace_file_e $trace_file_f $trace_file_g $trace_file_h
    # $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b --feeds $trace_feed_a $trace_feed_b
    # $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b --feeds $trace_feed_a $trace_feed_b
    # $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file $trace_file $trace_file $trace_file

    if [ $? -eq 0 ]; then
        echo "Simulator executed successfully."
    else
        echo "Simulator execution failed."
    fi
}

# Run the simulator with the provided trace and setup
run_simulator $TRACE_FILE_A $TRACE_FILE_B $TRACE_FILE_C $TRACE_FILE_D $TRACE_FILE_E $TRACE_FILE_F $TRACE_FILE_G $TRACE_FILE_H
