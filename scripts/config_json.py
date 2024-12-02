#!/usr/bin/python3
import json
import sys
import readline

def completer(text, state):
    """
    Custom completer function for tab completion.
    """
    options = [i for i in completer.options if i.startswith(text)]
    if state < len(options):
        return options[state]
    else:
        return None

def navigate_json(data, path=""):
    """
    Function to navigate through the JSON data structure so that the user can move to the desired item.
    """
    while True:
        # Print the current JSON location
        if isinstance(data, dict):
            print("\nCurrent location:", path if path else "Top level")
            print("Available keys:")
            for key in data.keys():
                print(f" - {key}")
            print(" (Q) Save and Exit")

            # Setup the completer options for tab completion
            completer.options = list(data.keys()) + ["Q", "q"]
            readline.set_completer(completer)
            readline.parse_and_bind("tab: complete")

            # Wait for user input
            choice = input("\nSelect an item to navigate to: ").strip()

            if choice.lower() == "q":
                return path, data
            elif choice in data:
                path += f".{choice}" if path else choice
                data = data[choice]
            else:
                print("Invalid input. Please select again.")
        
        elif isinstance(data, list):
            print("\nCurrent location:", path)
            print("Enter an index to select from the list (0 - {})".format(len(data) - 1))
            print(" (Q) Save and Exit")

            completer.options = [str(i) for i in range(len(data))] + ["Q", "q"]
            readline.set_completer(completer)
            readline.parse_and_bind("tab: complete")

            # Wait for user input
            choice = input("\nSelect an index: ").strip()

            if choice.lower() == "q":
                return path, data
            elif choice.isdigit() and int(choice) < len(data):
                path += f".{choice}" if path else choice
                data = data[int(choice)]
            else:
                print("Invalid input. Please select again.")
        else:
            # Enter the new value to modify the selected key
            return path, data


def modify_json(file_path):
    try:
        # Load JSON file
        with open(file_path, 'r') as file:
            data = json.load(file)
        
        # Select the item to modify through JSON navigation
        key_path, target_value = navigate_json(data)
        
        # Enter the new value for modification
        print(f"\nCurrent value: {target_value}")
        new_value = input("Enter a new value: ")

        # Convert new value if it's a number or a boolean
        try:
            new_value = int(new_value)
        except ValueError:
            try:
                new_value = float(new_value)
            except ValueError:
                if new_value.lower() == 'true':
                    new_value = True
                elif new_value.lower() == 'false':
                    new_value = False
        
        # Assign new value to the selected key
        keys = key_path.split(".")
        temp = data
        for key in keys[:-1]:
            temp = temp[int(key)] if key.isdigit() else temp[key]
        temp[keys[-1]] = new_value

        # Save the modified data back to the file
        with open(file_path, 'w') as file:
            json.dump(data, file, indent=4)
        
        print(f"The item {key_path} has been successfully updated to {new_value}.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    # Accept file path as command-line argument
    if len(sys.argv) != 2:
        print("Usage: python script.py <file_path>")
    else:
        file_path = sys.argv[1]
        modify_json(file_path)
