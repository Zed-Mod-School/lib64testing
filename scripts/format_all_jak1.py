import os
import subprocess

def run_formatter_on_gc_files(root_dir):
    for dirpath, _, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith('.gc'):
                gc_file_path = os.path.join(dirpath, filename)
                command = [
                    r"C:\Users\zedze\OneDrive\Documents\Github\lib64testing\out\build\Release\bin\formatter.exe",
                    "-w", "-f", gc_file_path
                ]

                # Print the exact command
                print("Running command:", " ".join(command))

                # Run the command
                try:
                    subprocess.run(command, check=True)
                    print(f"Formatted: {gc_file_path}")
                except subprocess.CalledProcessError as e:
                    print(f"Error formatting {gc_file_path}: {e}")
                except FileNotFoundError:
                    print("formatter.exe not found. Check the path.")
                    print(f"Error formatting {gc_file_path}: {e}")

# Choose *one* root directory by uncommenting the one you want
# Only the *last assignment* to root_directory will take effect
root_directory = r"C:\Users\zedze\OneDrive\Documents\Github\libsm64testing\goal_src\jak1"

run_formatter_on_gc_files(root_directory)
