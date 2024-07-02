#!/usr/bin/python3
import os
import re
from collections import defaultdict

def find_largest_byte_trace_files(directory):
    trace_files = defaultdict(lambda: (None, 0))
    pattern = re.compile(r'^(.*)-(\d+)B\.champsimtrace\.xz$')

    for filename in os.listdir(directory):
        match = pattern.match(filename)
        if match:
            trace_type = match.group(1)
            byte_size = int(match.group(2))
            current_max_file, current_max_size = trace_files[trace_type]

            if byte_size > current_max_size:
                trace_files[trace_type] = (filename, byte_size)

    for trace_type, (largest_file, largest_size) in trace_files.items():
        print(f"{trace_type}, {largest_file}")

# Define the directory path
directory_path = '/home/phw/sim/traces/downloads'

# Call the function
find_largest_byte_trace_files(directory_path)
