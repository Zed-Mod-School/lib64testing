import os
import trimesh
import tkinter as tk
from tkinter import filedialog, messagebox

def generate_sm64_surfaces_from_obj(obj_path, scale=50.0):
    mesh = trimesh.load_mesh(obj_path, process=True)
    if not hasattr(mesh, "triangles"):
        raise ValueError(f"Mesh '{obj_path}' has no triangles.")

    base_name = os.path.splitext(os.path.basename(obj_path))[0]
    output_path = os.path.join(os.path.dirname(obj_path), f"{base_name}_surfaces.c")

    with open(output_path, "w") as f:
        f.write('#include "level.h"\n\n')
        f.write('#include "../src/decomp/include/surface_terrains.h"\n\n')
        f.write(f"const struct SM64Surface {base_name}_surfaces[] = {{\n")

        for tri in mesh.triangles:
            v1 = [int(coord * scale) for coord in tri[0]]
            v2 = [int(coord * scale) for coord in tri[1]]
            v3 = [int(coord * scale) for coord in tri[2]]

            f.write(f"    {{SURFACE_DEFAULT, 0, TERRAIN_STONE, "
                    f"{{{{{v1[0]},{v1[1]},{v1[2]}}}, "
                    f"{{{v2[0]},{v2[1]},{v2[2]}}}, "
                    f"{{{v3[0]},{v3[1]},{v3[2]}}}}}}},\n")

        f.write("};\n")
        f.write(f"const size_t {base_name}_surfaces_count = "
                f"sizeof({base_name}_surfaces) / sizeof({base_name}_surfaces[0]);\n")

    return output_path

def select_folder_and_convert():
    folder = filedialog.askdirectory()
    if not folder:
        return

    generated_files = []
    errors = []

    for filename in os.listdir(folder):
        if filename.lower().endswith(".obj"):
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

# GUI
root = tk.Tk()
root.title("Batch OBJ to SM64Surface Generator")

tk.Label(root, text="Select a folder to process all .obj files:").pack(padx=10, pady=10)
tk.Button(root, text="Choose Folder and Convert", command=select_folder_and_convert).pack(pady=10)

root.mainloop()
