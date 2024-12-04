#!/bin/bash

# Define the root paths
SIM_ROOT=/home/phw/workspace/simulator/ChampSim_tma
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/602.gcc_s-2375B.champsimtrace.xz  # Sample trace file path
# TRACE_FEED=/home/phw/workspace/scripts/about_trace/parsed_bingo_bits/602.gcc_s-2375B_2411291532.csv
TRACE_FILE_A=/home/phw/workspace/traces/Champsim/spec/602.gcc_s-2375B.champsimtrace.xz  # Sample trace file path
TRACE_FEED_A=/home/phw/workspace/scripts/about_trace/parsed_bingo_bits/602.gcc_s-2375B_2411291532.csv
TRACE_FILE_B=/home/phw/workspace/traces/Champsim/spec/607.cactuBSSN_s-2421B.champsimtrace.xz  # Sample trace file path
TRACE_FEED_B=/home/phw/workspace/scripts/about_trace/parsed_bingo_bits/607.cactuBSSN_s-2421B_2411291532.csv
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/607.cactuBSSN_s-2421B.champsimtrace.xz  # Sample trace file path
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/649.fotonik3d_s-10881B.champsimtrace.xz  # Sample trace file path
# TRACE_FILE=/home/phw/workspace/traces/Champsim/spec/602.gcc_s-2375B.champsimtrace.xz
# LOG_FILE=/home/phw/workspace/simulator/logs/last_exp/sim_test.log

# Define the warmup and simulation instructions
NUM_WARMUP=10000000    # 10M instructions for warmup
NUM_SIMUL=25000000     # 25M instructions for simulation
# NUM_SIMUL=500000000     # 500M instructions for simulation

# Run the simulator with a sample trace
run_simulator() {
    local trace_file_a=$1
    local trace_feed_a=$2
    local trace_file_b=$3
    local trace_feed_b=$4
    echo "Running simulator with trace: $trace_file"
    echo "$SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b --feeds $trace_feed_a $trace_feed_b"

    # Execute the simulator and log the output
    # $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file
    # $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b --feeds $trace_feed_a $trace_feed_b
    $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file_a $trace_file_b --feeds $trace_feed_a $trace_feed_b
    # $SIM_ROOT/bin/champsim --warmup-instructions $NUM_WARMUP --simulation-instructions $NUM_SIMUL $trace_file $trace_file $trace_file $trace_file

    if [ $? -eq 0 ]; then
        echo "Simulator executed successfully."
    else
        echo "Simulator execution failed."
    fi
}

# Run the simulator with the provided trace and setup
run_simulator $TRACE_FILE_A $TRACE_FEED_A $TRACE_FILE_B $TRACE_FEED_B
