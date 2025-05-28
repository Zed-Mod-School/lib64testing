import os
import trimesh
import tkinter as tk
from tkinter import filedialog, messagebox

def generate_sm64_surfaces_from_obj(obj_path, scale=1.0):
    mesh = trimesh.load_mesh(obj_path, process=True)
    if not hasattr(mesh, "triangles"):
        raise ValueError("Mesh has no triangles. Is it a valid OBJ file?")

    output_path = os.path.splitext(obj_path)[0] + "_mario_surfaces.c"

    with open(output_path, "w") as f:
        f.write('#include "level.h"\n\n')
        f.write('#include "../src/decomp/include/surface_terrains.h"\n')
        f.write('const struct SM64Surface surfaces[] = {\n')

        for tri in mesh.triangles:
            v1 = [int(coord * scale) for coord in tri[0]]
            v2 = [int(coord * scale) for coord in tri[1]]
            v3 = [int(coord * scale) for coord in tri[2]]

            f.write('    {SURFACE_DEFAULT, 0, TERRAIN_STONE, {')
            f.write('{{{},{},{}}}, '.format(*v1))
            f.write('{{{},{},{}}}, '.format(*v2))
            f.write('{{{},{},{}}}}}}},\n'.format(*v3))

        f.write('};\n')
        f.write('const size_t surfaces_count = sizeof(surfaces) / sizeof(surfaces[0]);\n')

    return output_path

def select_file():
    filepath = filedialog.askopenfilename(filetypes=[("OBJ files", "*.obj")])
    if filepath:
        entry_path.delete(0, tk.END)
        entry_path.insert(0, filepath)

def run_conversion():
    path = entry_path.get()
    try:
        scale = float(entry_scale.get())
    except ValueError:
        messagebox.showerror("Invalid Input", "Scale must be a number.")
        return

    if not path or not os.path.isfile(path):
        messagebox.showerror("Missing File", "Please select a valid OBJ file.")
        return

    try:
        output = generate_sm64_surfaces_from_obj(path, scale)
        messagebox.showinfo("Success", f"Generated:\n{output}")
    except Exception as e:
        messagebox.showerror("Error", str(e))

# Tkinter GUI
root = tk.Tk()
root.title("OBJ to SM64Surface Generator")

tk.Label(root, text="OBJ File:").grid(row=0, column=0, padx=5, pady=5, sticky="e")
entry_path = tk.Entry(root, width=50)
entry_path.grid(row=0, column=1, padx=5, pady=5)
tk.Button(root, text="Browse", command=select_file).grid(row=0, column=2, padx=5, pady=5)

tk.Label(root, text="Scale:").grid(row=1, column=0, padx=5, pady=5, sticky="e")
entry_scale = tk.Entry(root)
entry_scale.insert(0, "1.0")
entry_scale.grid(row=1, column=1, padx=5, pady=5, sticky="w")

tk.Button(root, text="Generate C File", command=run_conversion, width=20).grid(row=2, column=1, pady=10)

root.mainloop()
