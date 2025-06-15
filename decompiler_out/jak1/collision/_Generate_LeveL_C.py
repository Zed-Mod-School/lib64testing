import os
import tkinter as tk
from tkinter import filedialog, messagebox
import re

def sanitize_material_name(name):
    """Convert material names to valid C identifiers."""
    name = re.sub(r'\W|^(?=\d)', '_', name)
    return name.upper()

def parse_obj_with_materials(obj_path):
    """Parses an OBJ file and returns a list of (material_name, triangle) tuples."""
    vertices = []
    triangles = []
    current_material = "SURFACE_DEFAULT"

    with open(obj_path, 'r') as f:
        lines = f.readlines()

    for line in lines:
        if line.startswith('v '):
            parts = line.strip().split()
            vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])
        elif line.startswith('usemtl '):
            current_material = sanitize_material_name(line.strip().split()[1])
            print(f"[INFO] Using material: {current_material}")
        elif line.startswith('f '):
            parts = line.strip().split()[1:]
            indices = [int(p.split('/')[0]) - 1 for p in parts]
            if len(indices) == 3:
                triangles.append((current_material, [vertices[i] for i in indices]))
            else:
                print(f"[WARN] Skipping non-triangle face in {obj_path}")

    return triangles

def generate_sm64_surfaces_from_obj(obj_path, scale=50.0):
    base_name = os.path.splitext(os.path.basename(obj_path))[0]
    output_path = os.path.join(os.path.dirname(obj_path), f"{base_name}_surfaces.c")
    triangles = parse_obj_with_materials(obj_path)

    with open(output_path, "w") as f:

        f.write(f"const struct SM64Surface {base_name}_surfaces[] = {{\n")

        for material, tri in triangles:
            v1 = [int(coord * scale) for coord in tri[0]]
            v2 = [int(coord * scale) for coord in tri[1]]
            v3 = [int(coord * scale) for coord in tri[2]]
            f.write(f"    {{{material}, 0, TERRAIN_STONE, "
                    f"{{{{{v1[0]},{v1[1]},{v1[2]}}}, "
                    f"{{{v2[0]},{v2[1]},{v2[2]}}}, "
                    f"{{{v3[0]},{v3[1]},{v3[2]}}}}}}},\n")

        f.write("};\n")
        f.write(f"const size_t {base_name}_surfaces_count = "
                f"sizeof({base_name}_surfaces) / sizeof({base_name}_surfaces[0]);\n")

    return output_path

def select_folder_and_show_options():
    folder = filedialog.askdirectory()
    if not folder:
        return

    obj_files.clear()
    for widget in file_frame.winfo_children():
        widget.destroy()

    all_files = [f for f in os.listdir(folder) if f.lower().endswith(".obj")]
    for filename in sorted(all_files):
        var = tk.BooleanVar(value=True)
        obj_files.append((filename, var))
        cb = tk.Checkbutton(file_frame, text=filename, variable=var, state="disabled")
        cb.pack(anchor="w")

    folder_path.set(folder)
    all_var.set(True)

def toggle_all_selection():
    state = all_var.get()
    for _, var in obj_files:
        var.set(state)
    for cb in file_frame.winfo_children():
        cb.configure(state="disabled" if state else "normal")

def run_conversion():
    folder = folder_path.get()
    if not folder:
        return

    selected_files = [f for f, var in obj_files if var.get()]
    if not selected_files:
        messagebox.showwarning("No Selection", "No OBJ files selected.")
        return

    generated_files = []
    errors = []

    for filename in selected_files:
        full_path = os.path.join(folder, filename)
        try:
            output = generate_sm64_surfaces_from_obj(full_path)
            generated_files.append(output)
        except Exception as e:
            errors.append(f"{filename}: {e}")

    if generated_files:
        messagebox.showinfo("Success", f"Generated:\n" + "\n".join(generated_files))
    else:
        messagebox.showwarning("No Files", "No OBJ files were processed.")

    if errors:
        messagebox.showerror("Errors", "\n".join(errors))

# GUI setup
root = tk.Tk()
root.title("Batch OBJ to SM64Surface Generator")

folder_path = tk.StringVar()
obj_files = []

tk.Label(root, text="Select a folder to process .obj files:").pack(padx=10, pady=(10, 0))
tk.Button(root, text="Choose Folder", command=select_folder_and_show_options).pack(pady=(0, 10))

all_var = tk.BooleanVar(value=True)
tk.Checkbutton(root, text="Process All (default)", variable=all_var, command=toggle_all_selection).pack()

file_frame = tk.Frame(root)
file_frame.pack(padx=10, pady=5, fill="both")

tk.Button(root, text="Convert Selected", command=run_conversion).pack(pady=10)

root.mainloop()
