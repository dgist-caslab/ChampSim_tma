# common.py
import json
import subprocess
import os


def check_bin_exists(memory):
    for root, dirs, files in os.walk("./bin"):
        if f"champsim_{memory}" in files:
            return True
    return False

def save_bin(name):
    command = ["mv", "bin/champsim", f"bin/champsim_{name}"]
    try:
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        # print("Command Output:", result.stdout)
        # print("Command Error (if any):", result.stderr)
        return True
    except subprocess.CalledProcessError as e:
        # print(f"Error occurred while executing the command: {e}")
        # print("Error Output:", e.stderr)
        return False

def do_make():
    command = ["make", "-j24"]
    try:
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        # print("Command Output:", result.stdout)
        # print("Command Error (if any):", result.stderr)
        return True
    except subprocess.CalledProcessError as e:
        # print(f"Error occurred while executing the command: {e}")
        # print("Error Output:", e.stderr)
        return False

def config_champsim():
    # The command we want to execute
    command = ["./config.sh", "tma_config.json"]

    try:
        # Running the command using subprocess.run
        result = subprocess.run(command, check=True, text=True, capture_output=True)
        
        # If needed, print the stdout and stderr of the command
        # print("Command Output:", result.stdout)
        # print("Command Error (if any):", result.stderr)
        return True 
        
    except subprocess.CalledProcessError as e:
        # Handle errors in case the command fails
        # print(f"Error occurred while executing the command: {e}")
        # print("Error Output:", e.stderr)
        return False

def update_physical_fast_memory_rows(file_path, new_value):
    """
    Opens a JSON file, updates the 'rows' value in 'physical_fast_memory', 
    and writes the updated content back to the file.

    :param file_path: Path to the JSON file.
    :param new_value: New value to set for 'rows' in 'physical_fast_memory'.
    """
    try:
        with open(file_path, 'r') as file:
            data = json.load(file)

        if 'physical_fast_memory' in data:
            data['physical_fast_memory']['rows'] = new_value
        else:
            raise KeyError("'physical_fast_memory' key does not exist in the JSON file.")

        with open(file_path, 'w') as file:
            json.dump(data, file, indent=4)

        # print("Updated 'physical_fast_memory'->'rows' successfully.")
        return True
    except FileNotFoundError:
        # print(f"Error: File '{file_path}' not found.")
        return False
    except KeyError as e:
        # print(f"Error: {e}")
        return False
    except json.JSONDecodeError:
        # print("Error: Invalid JSON format.")
        return False
