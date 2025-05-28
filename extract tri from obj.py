import tkinter as tk
from tkinter import filedialog, messagebox
import json

SCALE = 50.0

def parse_obj_and_scale(filepath):
    vertices = []
    triangles = []

    with open(filepath, 'r') as f:
        for line in f:
            if line.startswith('v '):  # Vertex
                _, x, y, z = line.strip().split()
                vertices.append([float(x) * SCALE, float(y) * SCALE, float(z) * SCALE])
            elif line.startswith('f '):  # Face
                parts = line.strip().split()[1:]
                if len(parts) == 3:
                    tri = []
                    for part in parts:
                        index = int(part.split('/')[0]) - 1  # OBJ indices are 1-based
                        tri.append(vertices[index])
                    triangles.append(tri)
    return triangles

def export_to_json(triangles, output_path):
    with open(output_path, 'w') as f:
        json.dump(triangles, f, indent=2)
    messagebox.showinfo("Done", f"Exported {len(triangles)} triangles to:\n{output_path}")

def select_file():
    filepath = filedialog.askopenfilename(
        title="Select .obj File",
        filetypes=[("OBJ Files", "*.obj")]
    )
    if not filepath:
        return

    try:
        triangles = parse_obj_and_scale(filepath)
        output_path = filedialog.asksaveasfilename(
            title="Save JSON Output",
            defaultextension=".json",
            filetypes=[("JSON Files", "*.json")]
        )
        if output_path:
            export_to_json(triangles, output_path)
    except Exception as e:
        messagebox.showerror("Error", f"Failed to process file:\n{e}")

def main():
    root = tk.Tk()
    root.title("OBJ to Scaled JSON Triangles")
    root.geometry("400x200")

    label = tk.Label(root, text="Select a .obj file to scale and export triangles to JSON.", wraplength=350, pady=20)
    label.pack()

    btn = tk.Button(root, text="Choose .obj File", command=select_file, height=2, width=20)
    btn.pack(pady=10)

    root.mainloop()

if __name__ == "__main__":
    main()
